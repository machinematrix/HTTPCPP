#include "Common.h"
#include <stdexcept>

void WinsockLoader::startup()
{
	#ifdef _WIN32
	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
		throw std::runtime_error("Error initializing Winsock DLL");
	#endif
}

WinsockLoader::WinsockLoader()
{
	startup();
}

WinsockLoader::~WinsockLoader()
{
	#ifdef _WIN32
	WSACleanup();
	#endif
}

WinsockLoader::WinsockLoader(const WinsockLoader&)
	:WinsockLoader()
{}

WinsockLoader& WinsockLoader::operator=(const WinsockLoader&)
{
	startup();
	return *this;
}