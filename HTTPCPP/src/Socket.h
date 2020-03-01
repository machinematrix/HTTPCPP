#ifndef __SOCKET__
#define __SOCKET__
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

#ifdef _WIN32
#include <winsock2.h>
using PollFileDescriptor = WSAPOLLFD;
using DescriptorType = SOCKET;
#elif defined __linux__
#include <poll.h>
using PollFileDescriptor = pollfd;
using DescriptorType = int;
#endif

class WinsockLoader;

class Socket
{
	friend class SocketPoller;
	friend bool operator!=(const Socket&, const Socket&) noexcept;
	friend bool operator<(const Socket&, const Socket&) noexcept;
	std::unique_ptr<WinsockLoader> loader;
	DescriptorType mSock;
	int domain, type, protocol;
public:
	Socket(DescriptorType);
	Socket(int domain, int type, int protocol);
	Socket(const Socket&) = delete;
	Socket(Socket&&) noexcept;
	~Socket();

	Socket& operator=(const Socket&) = delete;
	Socket& operator=(Socket&&) noexcept;

	void close();
	void bind(std::string_view address, short port, bool numericAddress);
	void listen(int queueLength);
	void toggleBlocking(bool toggle);
	Socket accept();
	std::int64_t receive(void *buffer, size_t bufferSize, int flags);
	std::int64_t send(void *buffer, size_t bufferSize, int flags);
};

bool operator!=(const Socket&, const Socket&) noexcept;
bool operator==(const Socket&, const Socket&) noexcept;
bool operator<(const Socket&, const Socket&) noexcept;

class SocketPoller
{
	std::vector<std::shared_ptr<Socket>> mSockets;
	std::vector<PollFileDescriptor> mPollFdList;
public:
	SocketPoller() = default;
	SocketPoller(const SocketPoller&) = delete;
	SocketPoller(SocketPoller&&) noexcept;

	SocketPoller& operator=(const SocketPoller&) = delete;
	SocketPoller& operator=(SocketPoller&&) noexcept;

	void addSocket(decltype(mSockets)::value_type socket, decltype(PollFileDescriptor::events) events);
	void removeSocket(const decltype(mSockets)::value_type::element_type &socket);
	void poll(int timeout, std::function<void(decltype(mSockets)::value_type, decltype(PollFileDescriptor::revents))> callback);
};

#endif