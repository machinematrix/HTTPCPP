#ifndef __NAMES__
#define __NAMES__

#ifdef __linux__
#define _GNU_SOURCE
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
#pragma comment(lib,"ws2_32.lib")
using ssize_t = SSIZE_T;
using DescriptorType = SOCKET;
inline void CloseSocket(DescriptorType mSock)
{
	closesocket(mSock);
}
#endif

//calls WSAStartup on construction and WSACleanup on destruction
class WinsockLoader
{
	static void startup();
public:
	WinsockLoader();
	~WinsockLoader();
	WinsockLoader(const WinsockLoader&);
	WinsockLoader(WinsockLoader&&) = default;
	WinsockLoader& operator=(const WinsockLoader&);
	WinsockLoader& operator=(WinsockLoader&&) = default;
};

namespace Http
{
	struct SocketWrapper
	{
		DescriptorType mSock;
	};
}

#endif