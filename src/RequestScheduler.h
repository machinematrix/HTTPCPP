#ifndef __REQUEST_SCHEDULER__
#define __REQUEST_SCHEDULER__
#include <functional>
#include <vector>
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

class ThreadPoolRequestScheduler : public IRequestScheduler
{
	ThreadPool mPool;
	DescriptorType mServerSocket;
public:
	ThreadPoolRequestScheduler(unsigned threadCount, DescriptorType serverSocket);
	~ThreadPoolRequestScheduler();
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

class SelectRequestScheduler : public IRequestScheduler
{
	DescriptorType mServerSocket;
	DescriptorType mMaxFileDescriptor;
	fd_set descriptors;
public:
	SelectRequestScheduler(DescriptorType serverSocket);
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

class PollRequestScheduler : public IRequestScheduler
{
	#ifdef _WIN32
	using PollFileDescriptor = WSAPOLLFD;
	#elif defined(__linux__)
	using PollFileDescriptor = pollfd;
	#endif

	std::vector<PollFileDescriptor> mSockets;
public:
	PollRequestScheduler(DescriptorType serverSocket);
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
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
		std::atomic<bool> isBeingServed;
		std::chrono::steady_clock::time_point creationTimePoint;

		SocketInfo(const std::chrono::steady_clock::time_point &creation);
		SocketInfo(const SocketInfo&);
		SocketInfo& operator=(const SocketInfo&);
	};

	ThreadPool pool;
	std::vector<PollFileDescriptor> mSockets;
	std::vector<SocketInfo> socketInfo; //in tuple: time_point is the time at which the socket was created. atomic flag indicates whether a worker thread is reading from that socket.
	std::chrono::milliseconds mSocketTimeToLive; //in milliseconds. socket will automatically close if there are no incoming connections for at least this amount of time

	void addToThreadPool(const std::function<void(DescriptorType)> &callback, decltype(socketInfo)::size_type index);
public:
	RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t mSocketTimeToLive);
	~RequestScheduler();
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

#endif