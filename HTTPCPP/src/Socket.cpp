#include "Socket.h"
#include <stdexcept>
#include <memory>

#ifdef _WIN32
#pragma comment(lib, "Secur32.lib")
#endif

namespace
{
	std::string formatMessage(unsigned msg)
	{
		std::string result;

		#ifdef _WIN32
		LPSTR message;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, static_cast<DWORD>(msg), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
		result = message;
		LocalFree(message);
		#elif defined(__linux__)
		result = strerror(msg);
		#endif

		return result;
	}

	inline void checkReturn(int ret)
	{
		if (ret == SOCKET_ERROR)
			#ifdef _WIN32
			throw std::runtime_error(formatMessage(WSAGetLastError()));
			#elif defined (__linux__)
			throw std::runtime_error(formatMessage(errno));
			#endif
	}

	#ifdef _WIN32
	using BufferType = char*;
	using LengthType = int;
	#elif defined(__linux__)
	using BufferType = void*;
	using LengthType = size_t;
	#endif
}

Socket::Socket(DescriptorType sock)
	:mSock(sock),
	domain(0),
	type(0),
	protocol(0)
{
	sockaddr name;
	int nameLen = 0, typeLen = sizeof(type);

	try
	{
		checkReturn(getsockname(mSock, &name, &nameLen));
		checkReturn(getsockopt(mSock, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &typeLen));

		domain = name.sa_family;
	}
	catch (const std::runtime_error&)
	{
		close();
		throw;
	}
}

Socket::Socket(int domain, int type, int protocol)
	:mSock(socket(domain, type, protocol)),
	domain(domain),
	type(type),
	protocol(protocol)
{
	if (mSock == INVALID_SOCKET)
		#ifdef _WIN32
		throw std::runtime_error(formatMessage(WSAGetLastError()));
		#elif defined (__linux__)
		throw std::runtime_error(errno);
		#endif

	try
	{
		char optval[8] = {};

		checkReturn(setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(optval)));
	}
	catch (const std::runtime_error&)
	{
		close();
		throw;
	}
}

Socket::Socket(Socket &&other) noexcept
	:mSock(other.mSock),
	loader(std::move(other.loader)),
	domain(other.domain),
	type(other.type),
	protocol(other.protocol)
{
	other.mSock = INVALID_SOCKET;
}

Socket::~Socket()
{
	close();
}

Socket& Socket::operator=(Socket &&other) noexcept
{
	mSock = other.mSock;
	loader = std::move(other.loader);
	other.mSock = INVALID_SOCKET;
	domain = other.domain;
	type = other.type;
	protocol = other.protocol;
}

void Socket::close()
{
	#ifdef _WIN32
	closesocket(mSock);
	#elif defined (__linux__)
	close(mSock);
	#endif
}

void Socket::bind(std::string_view address, short port, bool numericAddress)
{
	std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addressListPtr(nullptr, freeaddrinfo);

	addrinfo *list = NULL, hint = { 0 };
	hint.ai_flags = (numericAddress ? AI_NUMERICSERV | AI_PASSIVE : AI_PASSIVE);
	hint.ai_family = domain; //IPv4
	hint.ai_socktype = type;
	hint.ai_protocol = protocol;
	checkReturn(getaddrinfo(nullptr, std::to_string(port).c_str(), &hint, &list));
	addressListPtr.reset(list);

	auto len = addressListPtr->ai_addrlen;

	checkReturn(::bind(mSock, addressListPtr->ai_addr, static_cast<int>(len)));
}

void Socket::listen(int queueLength)
{
	checkReturn(::listen(mSock, queueLength));
}

void Socket::toggleBlocking(bool toggle)
{
	u_long toggleLong = toggle;
	checkReturn(ioctlsocket(mSock, FIONBIO, &toggleLong));
}

Socket Socket::accept()
{
	DescriptorType clientSocket = ::accept(mSock, nullptr, nullptr);

	if (clientSocket != SOCKET_ERROR)
		return clientSocket;
	else
		#ifdef _WIN32
		throw std::runtime_error(formatMessage(WSAGetLastError()));
		#elif defined (__linux__)
		throw std::runtime_error(formatMessage(errno));
		#endif
}

std::int64_t Socket::receive(void *buffer, size_t bufferSize, int flags)
{
	std::int64_t result = recv(mSock, static_cast<BufferType>(buffer), static_cast<LengthType>(bufferSize), flags);

	checkReturn(static_cast<int>(result));

	return result;
}