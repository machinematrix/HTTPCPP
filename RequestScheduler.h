#ifndef __REQUEST_SCHDULER__
#define __REQUEST_SCHDULER__
#include <functional>
#include <vector>
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

	ThreadPool pool;
	std::vector<PollFileDescriptor> mSockets;
	std::uint32_t socketTimeout; //in milliseconds. socket will automatically close if there are no incoming connections for at least this amount of time
public:
	RequestScheduler(DescriptorType serverSocket, unsigned threadCount, std::uint32_t socketTimeout);
	~RequestScheduler();
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

#endif