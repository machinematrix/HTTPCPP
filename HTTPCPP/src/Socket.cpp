#include "Socket.h"
#include <stdexcept>
#include <algorithm>
#include <string>

#ifdef _WIN32
#pragma comment(lib, "Secur32.lib")
#include <Ws2tcpip.h>
#elif defined __linux__
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <cstring>
#include <fcntl.h>
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#endif

namespace
{
	std::string formatMessage(int msg)
	{
		std::string result;

		#ifdef _WIN32
		LPSTR message;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, static_cast<DWORD>(msg), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
		result = message;
		LocalFree(message);
		#elif defined(__linux__)
		result = std::strerror(msg);
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

//calls WSAStartup on construction and WSACleanup on destruction
class WinsockLoader
{
	static void startup()
	{
		#ifdef _WIN32
		WSADATA wsaData;
		if (auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData))
			throw std::runtime_error(formatMessage(ret));
		#endif
	}
public:
	WinsockLoader()
	{
		startup();
	}

	~WinsockLoader()
	{
		#ifdef _WIN32
		WSACleanup();
		#endif
	}

	WinsockLoader(const WinsockLoader&)
		:WinsockLoader() //delegating constructor
	{}

	WinsockLoader(WinsockLoader&&) noexcept = default;

	WinsockLoader& operator=(const WinsockLoader&)
	{
		startup();
		return *this;
	}

	WinsockLoader& operator=(WinsockLoader&&) noexcept = default;
};

Socket::Socket(DescriptorType sock)
	:loader(new WinsockLoader),
	mSock(sock),
	domain(0),
	type(0),
	protocol(0)
{
	#ifdef	_WIN32
	using StructLength = int;
	#elif defined __linux__
	using StructLength = socklen_t;
	#endif
	sockaddr name;
	StructLength nameLen = sizeof(name), typeLen = sizeof(type);

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
	:loader(new WinsockLoader),
	mSock(socket(domain, type, protocol)),
	domain(domain),
	type(type),
	protocol(protocol)
{
	if (mSock == INVALID_SOCKET)
		#ifdef _WIN32
		throw std::runtime_error(formatMessage(WSAGetLastError()));
		#elif defined (__linux__)
		throw std::runtime_error(formatMessage(errno));
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

	return *this;
}

void Socket::close()
{
	#ifdef _WIN32
	closesocket(mSock);
	#elif defined (__linux__)
	::close(mSock);
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
	
	#ifdef _WIN32
	u_long toggleLong = !toggle;
	checkReturn(ioctlsocket(mSock, FIONBIO, &toggleLong));
	#else
	int flags = fcntl(mSock, F_GETFL, 0);
	checkReturn(flags);
	flags = toggle ? flags & ~O_NONBLOCK : flags | O_NONBLOCK;
	checkReturn(fcntl(mSock, F_SETFL, flags));
	#endif
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

std::int64_t Socket::send(void *buffer, size_t bufferSize, int flags)
{
	std::int64_t result = ::send(mSock, static_cast<BufferType>(buffer), static_cast<LengthType>(bufferSize), flags);

	checkReturn(static_cast<int>(result));

	return result;
}

bool operator!=(const Socket &lhs, const Socket &rhs) noexcept
{
	return lhs.mSock != rhs.mSock;
}

bool operator==(const Socket &lhs, const Socket &rhs) noexcept
{
	return !(lhs != rhs);
}

bool operator<(const Socket &lhs, const Socket &rhs) noexcept
{
	return lhs.mSock < rhs.mSock;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

SocketPoller::SocketPoller(SocketPoller&&) noexcept = default;

SocketPoller& SocketPoller::operator=(SocketPoller&&) noexcept = default;

void SocketPoller::addSocket(decltype(mSockets)::value_type socket, decltype(PollFileDescriptor::events) events)
{
	mSockets.emplace_back(socket);
	mPollFdList.push_back({ socket->mSock, events });
}

void SocketPoller::removeSocket(const decltype(mSockets)::value_type::element_type &socket)
{
	auto descriptor = socket.mSock;
	auto socketIt = std::find_if(mSockets.begin(), mSockets.end(), [&socket](const decltype(mSockets)::value_type &ptr) -> bool { return *ptr == socket; });
	auto entryIt = std::find_if(mPollFdList.begin(), mPollFdList.end(), [descriptor](const PollFileDescriptor &pollEntry) -> bool { return descriptor == pollEntry.fd; });

	if (socketIt != mSockets.end() && entryIt != mPollFdList.end())
	{
		mSockets.erase(socketIt);
		mPollFdList.erase(entryIt);
	}
	else
		throw std::out_of_range("Socket not found");
}

void SocketPoller::poll(int timeout, std::function<void(decltype(mSockets)::value_type, decltype(PollFileDescriptor::revents))> callback)
{
	#ifdef _WIN32
	int result = WSAPoll(mPollFdList.data(), static_cast<ULONG>(mPollFdList.size()), timeout);
	#elif defined (__linux__)
	int result = ::poll(mPollFdList.data(), mPollFdList.size(), timeout);
	#endif

	if (!mSockets.empty() && !mPollFdList.empty())
		for (std::int64_t i = static_cast<std::int64_t>(mPollFdList.size() - 1); i >= 0; --i)
			callback(mSockets[i], mPollFdList[i].revents);
}