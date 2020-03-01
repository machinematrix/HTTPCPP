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
#elif defined(_WIN32)
#include <Ws2tcpip.h>
#include <winsock2.h>
using ssize_t = SSIZE_T;
using DescriptorType = SOCKET;
#endif
#include <algorithm>
#include <string>

inline bool CaseInsensitiveComparator(const std::string &lhs, const std::string &rhs)
{
	return std::lexicographical_compare(lhs.cbegin(),
										lhs.cend(),
										rhs.cbegin(),
										rhs.cend(),
										[](char lhs, char rhs) -> bool { return std::toupper(lhs) < std::toupper(rhs); });
}

namespace Http
{
	struct SocketWrapper
	{
		DescriptorType mSock;
	};
}

#endif