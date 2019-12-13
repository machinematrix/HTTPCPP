#include "RequestScheduler.h"

ThreadPoolRequestScheduler::ThreadPoolRequestScheduler(unsigned threadCount, DescriptorType serverSocket)
	:mPool(threadCount)
	,mServerSocket(serverSocket)
{}

ThreadPoolRequestScheduler::~ThreadPoolRequestScheduler()
{
	mPool.waitForTasks();
}

void ThreadPoolRequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	DescriptorType newConnection = accept(mServerSocket, nullptr, nullptr);

	if (newConnection != INVALID_SOCKET)
	{
		mPool.addTask(std::bind(callback, newConnection));
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

SelectRequestScheduler::SelectRequestScheduler(DescriptorType serverSocket)
	:mServerSocket(serverSocket)
	,mMaxFileDescriptor(serverSocket)
{
	FD_ZERO(&descriptors);
	FD_SET(serverSocket, &descriptors);
}

void SelectRequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	if (select((int)mMaxFileDescriptor + 1, &descriptors, nullptr, nullptr, nullptr) != SOCKET_ERROR)
	{
		for (DescriptorType i = 0; i <= mMaxFileDescriptor; ++i)
		{
			if (FD_ISSET(i, &descriptors))
			{
				if (i == mServerSocket)
				{ 
					DescriptorType newFileDescriptor = accept(i, nullptr, nullptr);
					if (newFileDescriptor != SOCKET_ERROR)
					{
						if (newFileDescriptor > mMaxFileDescriptor)
							mMaxFileDescriptor = newFileDescriptor;
						FD_SET(newFileDescriptor, &descriptors);
					}
				}
				else {
					callback(i);
					FD_CLR(i, &descriptors);
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

PollRequestScheduler::PollRequestScheduler(DescriptorType serverSocket)
	:mSockets(1, PollFileDescriptor{ serverSocket, POLLIN })
{}

void PollRequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	#ifdef _WIN32
	int result = WSAPoll(mSockets.data(), static_cast<ULONG>(mSockets.size()), -1);
	#elif defined (__linux__)
	int result = poll(mSockets.data(), mSockets.size(), -1);
	#endif

	if (result != SOCKET_ERROR && result > 0)
	{
		for (decltype(mSockets)::size_type i = 0; i < mSockets.size();)
		{
			if (i && (mSockets[i].revents & POLLHUP)) //if the other side disconnected, close the socket.
			{
				CloseSocket(mSockets[i].fd);
				mSockets.erase(mSockets.begin() + i);
			}
			else
			{
				if (mSockets[i].revents & POLLIN)
				{
					if (!i) //if it's the server socket
					{
						DescriptorType clientSocket = accept(mSockets[i].fd, nullptr, nullptr);
						if (clientSocket != SOCKET_ERROR) {
							mSockets.push_back({ clientSocket, POLLIN });
							++i;
						}
					}
					else
					{
						callback(mSockets[i].fd);
						mSockets.erase(mSockets.begin() + i);
					}
				}
				else
					++i;
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RequestScheduler::RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t socketTimeout)
	:pool(threadCount),
	 mSockets(1, PollFileDescriptor{ serverSocket, POLLIN }),
	 socketTimeout(socketTimeout)
{}

RequestScheduler::~RequestScheduler()
{
	pool.waitForTasks();
	for (decltype(mSockets)::size_type i = 1, sz = mSockets.size(); i < sz; ++i) { //close every socket except the one on listening mode
		CloseSocket(mSockets[i].fd);
	}
}

// TODO: implement socket timeout
void RequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	#ifdef _WIN32
	int result = WSAPoll(mSockets.data(), static_cast<ULONG>(mSockets.size()), -1);
	#elif defined (__linux__)
	int result = poll(mSockets.data(), mSockets.size(), -1);
	#endif

	if (result != SOCKET_ERROR && result > 0)
	{
		for (decltype(mSockets)::size_type i = 0; i < mSockets.size();)
		{
			if (mSockets[i].revents & POLLHUP && i) //if the other side disconnected, close the socket.
			{
				CloseSocket(mSockets[i].fd);
				mSockets.erase(mSockets.begin() + i);
			}
			else if (mSockets[i].revents & POLLERR && i) //if I closed the after handling the request, stop monitoring it 
			{
				mSockets.erase(mSockets.begin() + i);
			}
			else if (mSockets[i].revents & POLLIN)
			{
				if (!i) //if it's the server socket
				{
					DescriptorType clientSocket = accept(mSockets[i].fd, nullptr, nullptr);
					if (clientSocket != SOCKET_ERROR) {
						mSockets.push_back({ clientSocket, POLLIN });
					}
				}
				else
				{
					callback(mSockets[i].fd); // TODO: find a way to use thread pool: a socket that is being served might still wake up WSAPoll because the other thread might not be over reading from it yet
					//pool.addTask(std::bind(callback, mSockets[i].fd));
					//mSockets.erase(it); //can't do this: socket could still be open after handling request due to keep-alive
				}
				++i;
			}
			else
				++i;
		}
	}
}