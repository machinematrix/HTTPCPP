#include "RequestScheduler.h"

ThreadPoolRequestScheduler::ThreadPoolRequestScheduler(unsigned threadCount, DescriptorType mServerSocket)
	:mPool(threadCount)
	,mServerSocket(mServerSocket)
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

SelectRequestScheduler::SelectRequestScheduler(DescriptorType mServerSocket)
	:mServerSocket(mServerSocket)
	,mMaxFileDescriptor(mServerSocket)
{
	FD_ZERO(&descriptors);
	FD_SET(mServerSocket, &descriptors);
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

PollRequestScheduler::PollRequestScheduler(DescriptorType mServerSocket)
	:mSockets(1, PollFileDescriptor{ mServerSocket, POLLIN })
{}

void PollRequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	#ifdef _WIN32
	int result = WSAPoll(mSockets.data(), mSockets.size(), -1);
	#elif defined (__linux__)
	int result = poll(sockets.data(), sockets.size(), -1);
	#endif

	if (result != SOCKET_ERROR && result > 0)
	{
		for (decltype(mSockets)::size_type i = 0; i < mSockets.size();)
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