#ifndef __REQUEST_SCHEDULER__
#define __REQUEST_SCHEDULER__
#include <vector>
#include <map>
#include <chrono>
#include <tuple>
#include <atomic>
#include "Common.h"
#include "ThreadPool.h"

#ifdef __linux__
#include <poll.h>
#endif

class IRequestScheduler
{
public:
	virtual ~IRequestScheduler() = default;
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) = 0;
};

class RequestScheduler : public IRequestScheduler
{
	#ifdef _WIN32
	using PollFileDescriptor = WSAPOLLFD;
	#elif defined(__linux__)
	using PollFileDescriptor = pollfd;
	#endif

	struct SocketInfo
	{
		std::atomic<bool> mIsBeingServed;
		std::chrono::steady_clock::time_point mLastServedTimePoint;

		SocketInfo(const std::chrono::steady_clock::time_point &creation);
		SocketInfo(const SocketInfo&);
		SocketInfo& operator=(const SocketInfo&);
	};

	ThreadPool mPool;
	std::vector<PollFileDescriptor> mSockets;
	std::unordered_map<DescriptorType, SocketInfo> mSocketInfo; //in tuple: time_point is the time at which the socket was created. atomic flag indicates whether a worker thread is reading from that socket.
	std::chrono::milliseconds mSocketTimeToLive; //in milliseconds. socket will automatically close if there are no incoming connections for at least this amount of time

	void addToThreadPool(const std::function<void(DescriptorType)> &callback, decltype(mSocketInfo)::iterator it);
public:
	RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t mSocketTimeToLive);
	~RequestScheduler();
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

#endif