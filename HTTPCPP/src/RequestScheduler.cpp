#include "RequestScheduler.h"
#ifndef NDEBUG
#include <iostream>
#endif

void RequestScheduler::addToThreadPool(const std::function<void(DescriptorType)> &callback, decltype(mSocketInfo)::iterator it)
{
	callback(it->first);
	it->second.mLastServedTimePoint = std::chrono::steady_clock::now();
	it->second.mIsBeingServed.store(false);
}

RequestScheduler::RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t mSocketTimeToLive)
	:mPool(threadCount)
	,mSockets(1, PollFileDescriptor{ serverSocket, POLLIN })
	,mSocketInfo{ { serverSocket, std::chrono::steady_clock::now() } }
	,mSocketTimeToLive(mSocketTimeToLive)
{}

RequestScheduler::~RequestScheduler()
{
	mPool.waitForTasks();
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

	if (result == SOCKET_ERROR)
		return;

	for (/*decltype(mSockets)::size_type*/std::int64_t i = static_cast<std::int64_t>(mSockets.size() - 1); i >= 0; --i)
	{
		if (mSockets[i].revents & POLLIN)
		{
			if (!i) //if it's the server socket
			{
				DescriptorType clientSocket = accept(mSockets[i].fd, nullptr, nullptr);

				if (clientSocket != SOCKET_ERROR)
				{
					#ifndef NDEBUG
					cout << __func__ << ' ' << "accept() returned socket " << clientSocket << endl;
					#endif
					mSockets.push_back({ clientSocket, POLLIN });

					if (!mSocketInfo.emplace(clientSocket, steady_clock::now()).second)
					{
						#ifndef NDEBUG
						std::cout << __func__ << ' ' << "SOCKET RETURNED FROM accept() (" << clientSocket << ") ALREADY EXISTED ON THE MAP" << std::endl;
						#endif
					}
				}
			}
			else
			{
				auto it = mSocketInfo.find(mSockets[i].fd);

				if (!it->second.mIsBeingServed.load())
				{
					#ifndef NDEBUG
					cout << __func__ << ' ' << "Serving request with socket: " << mSockets[i].fd << endl;
					#endif

					it->second.mIsBeingServed.store(true);
					mPool.addTask(std::bind(&RequestScheduler::addToThreadPool, this, callback, it));
				}
			}
		}
		else if (i &&
				 mSockets[i].revents & POLLNVAL &&
				 !mSocketInfo.find(mSockets[i].fd)->second.mIsBeingServed.load()) //if I closed the socket after handling the request, stop monitoring it 
		{
			#ifndef NDEBUG
			cout << __func__ << ' ' << "Socket " << mSockets[i].fd << " was closed by me" << endl;
			#endif

			mSocketInfo.erase(mSocketInfo.find(mSockets[i].fd));
			mSockets.erase(mSockets.begin() + i);
		}
		else if (i &&
				 !mSocketInfo.find(mSockets[i].fd)->second.mIsBeingServed.load() &&
				 (mSockets[i].revents & POLLHUP || steady_clock::now() - mSocketInfo.find(mSockets[i].fd)->second.mLastServedTimePoint > mSocketTimeToLive)) //if the other side disconnected or if the sockets TTL has expired, close the socket.
		{
			#ifndef NDEBUG
			cout << __func__ << ' ' << "Socket " << mSockets[i].fd << " expired or got hung up, closing it..." << endl;
			#endif

			CloseSocket(mSockets[i].fd);
			mSocketInfo.erase(mSocketInfo.find(mSockets[i].fd));
			mSockets.erase(mSockets.begin() + i);
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