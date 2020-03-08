#include "Socket.h"
#include <algorithm>
#include <string>

#ifdef _WIN32
//#define SECURITY_WIN32
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
	std::string formatMessage(int code)
	{
		std::string result;

		#ifdef _WIN32
		LPSTR message;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, static_cast<DWORD>(code), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
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
			throw SocketException(WSAGetLastError());
			#elif defined (__linux__)
			throw SocketException(errno);
			#endif
	}

	#ifdef _WIN32
	using BufferType = char*;
	using LengthType = int;

	inline void checkSchannelReturn(SECURITY_STATUS ret)
	{
		if (ret != SEC_E_OK)
			throw SocketException(ret);
	}
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
			throw SocketException(ret);
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

SocketException::SocketException(int code)
	:std::runtime_error(formatMessage(code)),
	errorCode(code)
{}

int SocketException::getErrorCode() const
{
	return errorCode;
}

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
	sockaddr name = {};
	StructLength nameLen = sizeof(name), typeLen = sizeof(socklen_t);

	try
	{
		checkReturn(getsockname(mSock, &name, &nameLen));
		checkReturn(getsockopt(mSock, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &typeLen));

		domain = name.sa_family;
	}
	catch (const SocketException&)
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
		throw SocketException(WSAGetLastError());
		#elif defined (__linux__)
		throw SocketException(errno);
		#endif

	try
	{
		char optval[8] = {};

		checkReturn(setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(optval)));
	}
	catch (const SocketException&)
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

Socket* Socket::accept()
{
	DescriptorType clientSocket = ::accept(mSock, nullptr, nullptr);

	if (clientSocket != SOCKET_ERROR)
		return new Socket(clientSocket);
	else
		#ifdef _WIN32
		throw SocketException(WSAGetLastError());
		#elif defined (__linux__)
		throw SocketException(errno);
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void TLSSocket::setupContext()
{
	#ifdef _WIN32
	constexpr const char *packageName = "Negotiate";
	std::unique_ptr<char[]> buffer, outputBuffer;
	std::int64_t bytesRead = 0, bytesSent = 0;

	PSecPkgInfoA packageInfo;
	TimeStamp lifetime;
	SecBuffer OutSecBuff;
	SecBufferDesc OutBuffDesc = { 0, 1, &OutSecBuff };
	SecBuffer InSecBuff;
	SecBufferDesc InBuffDesc = { 0, 1, &InSecBuff };
	ULONG Attribs = 0;
	decltype(packageInfo->cbMaxToken) maxMessage;
	BOOL done = FALSE, newConversation = TRUE;
	SECURITY_STATUS result;
	SecPkgContext_Sizes SecPkgContextSizes;
	//SecPkgContext_NegotiationInfo SecPkgNegInfo;
	ULONG cbMaxSignature;
	ULONG cbSecurityTrailer;

	checkSchannelReturn(QuerySecurityPackageInfoA(const_cast<char*>(packageName), &packageInfo));
	maxMessage = packageInfo->cbMaxToken;
	FreeContextBuffer(packageInfo);

	buffer.reset(new char[maxMessage]);
	outputBuffer.reset(new char[maxMessage]);

	//AcceptAuthSocket
	InSecBuff.cbBuffer = OutSecBuff.cbBuffer = maxMessage;
	InSecBuff.BufferType = OutSecBuff.BufferType = SECBUFFER_TOKEN;
	InSecBuff.pvBuffer = buffer.get();
	OutSecBuff.pvBuffer = outputBuffer.get();

	checkSchannelReturn(AcquireCredentialsHandle(NULL, const_cast<char*>(packageName), SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &hCredentials, &lifetime));

	do
	{
		try {
			toggleBlocking(false);
			while ((bytesRead += Socket::receive(buffer.get(), maxMessage - bytesRead, 0)) < maxMessage);
			//Socket::receive(buffer.get(), maxMessage - bytesRead, 0);
			bytesRead = 0;
			toggleBlocking(true);
		}
		catch (const SocketException &e) {
			toggleBlocking(true);
			#ifdef _WIN32
			if (e.getErrorCode() != WSAEWOULDBLOCK)
			#elif defined(__linux__)
			if (e.getErrorCode() != EWOULDBLOCK)
			#endif
				throw;
		}

		checkSchannelReturn(result = AcceptSecurityContext(&hCredentials, newConversation ? nullptr : &hContext, &InBuffDesc, Attribs, SECURITY_NATIVE_DREP, &hContext, &OutBuffDesc, &Attribs, &lifetime));
		if ((SEC_I_COMPLETE_NEEDED == result) || (SEC_I_COMPLETE_AND_CONTINUE == result))
			checkSchannelReturn(CompleteAuthToken(&hContext, &OutBuffDesc));

		try {
			toggleBlocking(false);
			while ((bytesSent += Socket::receive(outputBuffer.get(), maxMessage - bytesSent, 0)) < maxMessage);
			bytesSent = 0;
			toggleBlocking(true);
		}
		catch (const SocketException &e) {
			toggleBlocking(true);
			#ifdef _WIN32
			if (e.getErrorCode() != WSAEWOULDBLOCK)
				#elif defined(__linux__)
			if (e.getErrorCode() != EWOULDBLOCK)
				#endif
				throw;
		}

		newConversation = FALSE;
	} while (!((result == SEC_I_CONTINUE_NEEDED) || (result == SEC_I_COMPLETE_AND_CONTINUE)));
	//AcceptAuthSocket

	checkSchannelReturn(QueryContextAttributes(&hContext, SECPKG_ATTR_SIZES, &SecPkgContextSizes));

	cbMaxSignature = SecPkgContextSizes.cbMaxSignature;
	cbSecurityTrailer = SecPkgContextSizes.cbSecurityTrailer;

	//checkSchannelReturn(QueryContextAttributes(&hContext, SECPKG_ATTR_NEGOTIATION_INFO, &SecPkgNegInfo));
	//FreeContextBuffer(SecPkgNegInfo.PackageInfo);
	//checkSchannelReturn(ImpersonateSecurityContext(&hContext));
	contextSetup = true;
	#endif
}

TLSSocket::TLSSocket(TLSSocket &&other) noexcept
	:Socket(std::move(other)),
	hCredentials(other.hCredentials),
	hContext(other.hContext)
{
	other.hCredentials = CredHandle{};
	other.hContext = SecHandle{};
}

TLSSocket::~TLSSocket()
{
	DeleteSecurityContext(&hContext);
	FreeCredentialHandle(&hCredentials);
}

TLSSocket& TLSSocket::operator=(TLSSocket &&other) noexcept
{
	*static_cast<Socket*>(this) = std::move(other);
	hCredentials = other.hCredentials;
	other.hCredentials = CredHandle{};
	hContext = other.hContext;
	other.hContext = SecHandle{};

	return *this;
}

TLSSocket* TLSSocket::accept()
{
	DescriptorType clientSocket = ::accept(mSock, nullptr, nullptr);

	if (clientSocket != SOCKET_ERROR)
		return new TLSSocket(clientSocket);
	else
		#ifdef _WIN32
		throw SocketException(WSAGetLastError());
	#elif defined (__linux__)
		throw SocketException(errno);
	#endif
}

std::int64_t TLSSocket::receive(void *buffer, size_t bufferSize, int flags)
{
	auto result = Socket::receive(buffer, bufferSize, flags);
	#ifdef _WIN32
	if (!contextSetup)
		setupContext();
	#endif
	return result;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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