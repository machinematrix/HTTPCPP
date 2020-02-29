#ifndef __NAMES__
#define __NAMES__

#ifdef __linux__
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
using DescriptorType = int;
inline void CloseSocket(DescriptorType sock)
{
	close(sock);
}
#elif defined(_WIN32)
#include <Ws2tcpip.h>
#include <winsock2.h>
using ssize_t = SSIZE_T;
using DescriptorType = SOCKET;
inline void CloseSocket(DescriptorType sock)
{
	closesocket(sock);
}
#endif
#include <algorithm>
#include <string>

//To avoid conversion warnings between integer types of different width
template<typename SizeType>
inline ssize_t MyRecv(DescriptorType sock, void *buffer, SizeType bufferSize, int flags)
{
	#ifdef _WIN32
	return recv(sock, static_cast<char*>(buffer), static_cast<int>(bufferSize), flags);
	#elif defined(__linux__)
	return recv(sock, buffer, static_cast<size_t>(bufferSize), flags);
	#endif
}

template<typename SizeType>
inline ssize_t MySend(DescriptorType sock, const void *buffer, SizeType bufferSize, int flags)
{
	#ifdef _WIN32
	return send(sock, static_cast<const char*>(buffer), static_cast<int>(bufferSize), flags);
	#elif defined(__linux__)
	return send(sock, buffer, static_cast<size_t>(bufferSize), flags);
	#endif
}

inline bool CaseInsensitiveComparator(const std::string &lhs, const std::string &rhs)
{
	return std::lexicographical_compare(lhs.cbegin(),
										lhs.cend(),
										rhs.cbegin(),
										rhs.cend(),
										[](char lhs, char rhs) -> bool { return std::toupper(lhs) < std::toupper(rhs); });
}

//calls WSAStartup on construction and WSACleanup on destruction
class WinsockLoader
{
	static void startup();
public:
	WinsockLoader();
	~WinsockLoader();
	WinsockLoader(const WinsockLoader&);
	WinsockLoader(WinsockLoader&&) noexcept;
	WinsockLoader& operator=(const WinsockLoader&);
	WinsockLoader& operator=(WinsockLoader&&) noexcept;
};

namespace Http
{
	struct SocketWrapper
	{
		DescriptorType mSock;
	};
}

#endif