#include "RequestScheduler.h"
#ifndef NDEBUG
#include <iostream>
#endif

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

void RequestScheduler::addToThreadPool(const std::function<void(DescriptorType)> &callback, decltype(mSocketInfo)::size_type index)
{
	callback(mSockets[index].fd);
	mSocketInfo[i].mLastServedTimePoint = std::chrono::steady_clock::now();
	mSocketInfo[index].mIsBeingServed.store(false);
}

RequestScheduler::RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t mSocketTimeToLive)
	:pool(threadCount)
	,mSockets(1, PollFileDescriptor{ serverSocket, POLLIN })
	,mSocketInfo(1, std::chrono::steady_clock::now())
	,mSocketTimeToLive(mSocketTimeToLive)
{}

RequestScheduler::~RequestScheduler()
{
	pool.waitForTasks();
	for (decltype(mSockets)::size_type i = 1, sz = mSockets.size(); i < sz; ++i) { //close every socket except the one on listening mode
		CloseSocket(mSockets[i].fd);
	}
}

void RequestScheduler::handleRequest(const std::function<void(DescriptorType)> &callback)
{
	using std::chrono::steady_clock;
	using std::chrono::milliseconds;
	using std::make_tuple;

	#ifndef NDEBUG
	using std::cout;
	using std::endl;
	#endif

	#ifdef _WIN32
	int result = WSAPoll(mSockets.data(), static_cast<ULONG>(mSockets.size()), 1000);
	#elif defined (__linux__)
	int result = poll(mSockets.data(), mSockets.size(), 1000);
	#endif

	if (result != SOCKET_ERROR)
	{
		for (decltype(mSockets)::size_type i = 0; i < mSockets.size();)
		{
			if (i && mSockets[i].revents & POLLNVAL && !mSocketInfo[i].mIsBeingServed.load()) //if I closed the socket after handling the request, stop monitoring it 
			{
				#ifndef NDEBUG
				cout << "Socket " << mSockets[i].fd << " was closed by me" << endl;
				#endif

				mSockets.erase(mSockets.begin() + i);
				mSocketInfo.erase(mSocketInfo.begin() + i);
			}
			else if(i && !mSocketInfo[i].mIsBeingServed.load() && (mSockets[i].revents & POLLHUP || steady_clock::now() - mSocketInfo[i].mLastServedTimePoint > mSocketTimeToLive)) //if the other side disconnected or if the sockets TTL has expired, close the socket.
			{
				#ifndef NDEBUG
				cout << "Socket " << mSockets[i].fd << " expired or got hung up, closing it..." << endl;
				#endif

				CloseSocket(mSockets[i].fd);
				mSockets.erase(mSockets.begin() + i);
				mSocketInfo.erase(mSocketInfo.begin() + i);
			}
			else if (mSockets[i].revents & POLLIN)
			{
				if (!i) //if it's the server socket
				{
					DescriptorType clientSocket = accept(mSockets[i].fd, nullptr, nullptr);
					if (clientSocket != SOCKET_ERROR) {
						#ifndef NDEBUG
						cout << "New socket: " << clientSocket << endl;
						#endif

						mSockets.push_back({ clientSocket, POLLIN });
						mSocketInfo.emplace_back(steady_clock::now());
					}
				}
				else
				{
					if (!mSocketInfo[i].mIsBeingServed.load())
					{
						#ifndef NDEBUG
						cout << "Serving request with socket: " << mSockets[i].fd << endl;
						#endif

						mSocketInfo[i].mIsBeingServed.store(true);
						pool.addTask(std::bind(&RequestScheduler::addToThreadPool, this, callback, i));
					}
				}
				++i;
			}
			else
				++i;
		}
	}
}

RequestScheduler::SocketInfo::SocketInfo(const std::chrono::steady_clock::time_point &creation)
	:mLastServedTimePoint(creation),
	mIsBeingServed(false)
{}

RequestScheduler::SocketInfo::SocketInfo(const SocketInfo &other)
	:mLastServedTimePoint(other.mLastServedTimePoint),
	mIsBeingServed(other.mIsBeingServed.load())
{}

RequestScheduler::SocketInfo& RequestScheduler::SocketInfo::operator=(const SocketInfo &other)
{
	mLastServedTimePoint = other.mLastServedTimePoint;
	mIsBeingServed.store(other.mIsBeingServed.load());

	return *this;
}