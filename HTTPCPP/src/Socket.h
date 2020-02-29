#ifndef __SOCKET__
#define __SOCKET__
#include <cstdint>
#include "Common.h"

class Socket
{
	WinsockLoader loader;
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
};

#endif