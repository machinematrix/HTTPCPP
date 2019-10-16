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
	ThreadPoolRequestScheduler(unsigned threadCount, DescriptorType mServerSocket);
	~ThreadPoolRequestScheduler();
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

class SelectRequestScheduler : public IRequestScheduler
{
	DescriptorType mServerSocket;
	DescriptorType mMaxFileDescriptor;
	fd_set descriptors;
public:
	SelectRequestScheduler(DescriptorType mServerSocket);
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
	PollRequestScheduler(DescriptorType mServerSocket);
	virtual void handleRequest(const std::function<void(DescriptorType)> &callback) override;
};

#endif