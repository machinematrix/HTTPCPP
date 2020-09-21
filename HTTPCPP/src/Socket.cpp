#include "Socket.h"
#include <algorithm>
#include <string>
#include <array>

#ifdef _WIN32
//#define SECURITY_WIN32
#include <Ws2tcpip.h>
#include <credssp.h>
#include <type_traits>
#include <Schnlsp.h>
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
#undef min
#undef max

#include <iostream>

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

	void checkSSPIReturn(SECURITY_STATUS ret)
	{
		//if (ret != SEC_E_OK)
		if (ret < 0)
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
	mDomain(0),
	mType(0),
	mProtocol(0)
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
		checkReturn(getsockopt(mSock, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&mType), &typeLen));

		mDomain = name.sa_family;
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
	mDomain(domain),
	mType(type),
	mProtocol(protocol)
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
	mDomain(other.mDomain),
	mType(other.mType),
	mProtocol(other.mProtocol)
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
	mDomain = other.mDomain;
	mType = other.mType;
	mProtocol = other.mProtocol;

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
	hint.ai_family = mDomain; //IPv4
	hint.ai_socktype = mType;
	hint.ai_protocol = mProtocol;
	checkReturn(getaddrinfo(nullptr, std::to_string(port).c_str(), &hint, &list));
	addressListPtr.reset(list);

	auto len = addressListPtr->ai_addrlen;

	checkReturn(::bind(mSock, addressListPtr->ai_addr, static_cast<int>(len)));
}

void Socket::listen(int queueLength)
{
	checkReturn(::listen(mSock, queueLength));
}

void Socket::toggleNonBlockingMode(bool toggle)
{
	#ifdef _WIN32
	//if (toggle != mNonBlocking)
	//{
	u_long toggleLong = mNonBlocking = toggle;
	checkReturn(ioctlsocket(mSock, FIONBIO, &toggleLong));
	//}
	#elif defined (__linux__)
	int flags = fcntl(mSock, F_GETFL, 0);
	checkReturn(flags);
	flags = toggle ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
	checkReturn(fcntl(mSock, F_SETFL, flags));
	#endif
}

bool Socket::isNonBlocking()
{
	#ifdef _WIN32
	return mNonBlocking;
	#elif defined (__linux__)
	int flags = fcntl(mSock, F_GETFL, 0);
	checkReturn(flags);
	return flags & O_NONBLOCK;
	#endif
}

void Socket::setSocketOption(int level, int optionName, const void *optionValue, int optionLength)
{
	#ifdef _WIN32
	setsockopt(mSock, level, optionName, static_cast<const char*>(optionValue), optionLength);
	#elif defined (__linux__)
	setsockopt(mSock, level, optionName, optionValue, static_cast<socklen_t>(optionLength));
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
	//certmgr.msc - certificate store
	#ifdef _WIN32
	/*unsigned long packageCount;
	PSecPkgInfoA packages;
	EnumerateSecurityPackagesA(&packageCount, &packages);

	for (decltype(packageCount) i = 0; i < packageCount; ++i)
		std::cout << packages[i].Name << std::endl;*/

	constexpr const char *packageName = "Schannel"/*UNISP_NAME_A*/;
	std::string buffer, outputBuffer;
	PSecPkgInfoA packageInfo;
	decltype(packageInfo->cbMaxToken) maxMessage, toRead = 5;
	TimeStamp lifetime;
	SecBuffer OutSecBuff;
	SecBufferDesc OutBuffDesc = { SECBUFFER_VERSION, 1, &OutSecBuff };
	SecBuffer InSecBuff[2];
	SecBufferDesc InBuffDesc = { SECBUFFER_VERSION, 2, InSecBuff };
	std::unique_ptr<std::remove_pointer<HCERTSTORE>::type, std::function<BOOL __stdcall(HCERTSTORE)>> certificateStore(nullptr, std::bind(CertCloseStore, std::placeholders::_1, CERT_CLOSE_STORE_FORCE_FLAG));
	std::unique_ptr<const CERT_CONTEXT, decltype(CertFreeCertificateContext)*> certificate(nullptr, CertFreeCertificateContext);
	SCHANNEL_CRED schannelCredential = {};
	std::int64_t bytesRead = 0;
	SECURITY_STATUS result;
	BOOL newConversation = TRUE;
	ULONG Attribs = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM;

	certificateStore.reset(CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"MY")); //try copying it to root
	if (!certificateStore)
		throw SocketException(GetLastError());

	certificate.reset(CertFindCertificateInStore(certificateStore.get(), X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_A, "localhost", NULL));
	if (!certificate)
		throw SocketException(GetLastError());
	
	auto certPtr = certificate.get();

	schannelCredential.dwVersion = SCHANNEL_CRED_VERSION;
	schannelCredential.cCreds = 1;
	schannelCredential.paCred = &certPtr;
	//schannelCredential.hRootStore = certificateStore.get();

	checkSSPIReturn(QuerySecurityPackageInfoA(const_cast<char*>(packageName), &packageInfo));
	maxMessage = packageInfo->cbMaxToken;
	FreeContextBuffer(packageInfo);

	buffer.resize(maxMessage, '\0');
	outputBuffer.resize(maxMessage, '\0');

	//AcceptAuthSocket

	checkSSPIReturn(AcquireCredentialsHandleA(nullptr, const_cast<char*>(packageName), SECPKG_CRED_BOTH, nullptr, &schannelCredential, nullptr, nullptr, &mCredentialsHandle, &lifetime));

	do
	{
		OutSecBuff.cbBuffer = maxMessage;
		OutSecBuff.pvBuffer = outputBuffer.data();
		InSecBuff[0].BufferType = OutSecBuff.BufferType = SECBUFFER_TOKEN;
		InSecBuff[0].pvBuffer = buffer.data();
		InSecBuff[1].BufferType = SECBUFFER_EMPTY;
		InSecBuff[1].pvBuffer = nullptr;
		InSecBuff[1].cbBuffer = 0;

		try {
			bytesRead += Socket::receive(buffer.data() + bytesRead, toRead, 0);

			InSecBuff[0].cbBuffer = bytesRead;
		}
		catch (const SocketException &e) {
			InSecBuff[0].cbBuffer = bytesRead;
			if (e.getErrorCode() != WSAEWOULDBLOCK && e.getErrorCode() != WSAETIMEDOUT)
				throw;
		}

		result = AcceptSecurityContext(&mCredentialsHandle, newConversation ? nullptr : &mContextHandle, &InBuffDesc, Attribs, SECURITY_NATIVE_DREP, &mContextHandle, &OutBuffDesc, &Attribs, &lifetime);
		
		switch (result)
		{
			case SEC_I_COMPLETE_NEEDED:
			case SEC_I_COMPLETE_AND_CONTINUE:
				checkSSPIReturn(CompleteAuthToken(&mContextHandle, &OutBuffDesc));
				[[fallthrough]];
			case SEC_E_OK:
			case SEC_I_CONTINUE_NEEDED:
			{
				if (OutSecBuff.BufferType == SECBUFFER_TOKEN)
				{
					try {
						std::int64_t bytesSent = 0;
						while ((bytesSent += Socket::send(OutSecBuff.pvBuffer, OutSecBuff.cbBuffer - bytesSent, 0)) < OutSecBuff.cbBuffer);
						std::cout << "Sent " << bytesSent << " bytes to client" << std::endl;
					}
					catch (const SocketException &e) {
						if (e.getErrorCode() != WSAEWOULDBLOCK)
							throw;
					}
				}

				toRead = 5;
				bytesRead = 0;
				break;
			}
			case SEC_E_INCOMPLETE_MESSAGE:
				if (InSecBuff[1].BufferType == SECBUFFER_MISSING)
					toRead = InSecBuff[1].cbBuffer;
				break;
			default:
				checkSSPIReturn(result);
				toRead = 5;
				break;
		}

		newConversation = FALSE;
	}
	while ((result == SEC_I_CONTINUE_NEEDED) || (result == SEC_I_COMPLETE_AND_CONTINUE) || result == SEC_E_INCOMPLETE_MESSAGE);
	//AcceptAuthSocket

	checkSSPIReturn(QueryContextAttributes(&mContextHandle, SECPKG_ATTR_STREAM_SIZES, &mStreamSizes));
	mContextSetup = true;
	#endif
}

TLSSocket::TLSSocket(TLSSocket &&other) noexcept
	:Socket(std::move(other)),
	mCredentialsHandle(other.mCredentialsHandle),
	mContextHandle(other.mContextHandle),
	mStreamSizes(other.mStreamSizes),
	mContextSetup(other.mContextSetup)
{
	other.mCredentialsHandle = CredHandle {};
	other.mContextHandle = SecHandle {};
}

TLSSocket::~TLSSocket()
{
	DeleteSecurityContext(&mContextHandle);
	FreeCredentialHandle(&mCredentialsHandle);
}

TLSSocket& TLSSocket::operator=(TLSSocket &&other) noexcept
{
	*static_cast<Socket*>(this) = std::move(other);
	mCredentialsHandle = other.mCredentialsHandle;
	other.mCredentialsHandle = CredHandle{};
	mContextHandle = other.mContextHandle;
	other.mContextHandle = SecHandle{};
	mStreamSizes = other.mStreamSizes;
	mContextSetup = other.mContextSetup;

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

std::string TLSSocket::receiveTLSMessage(int flags, std::string::size_type expected)
{
	std::string result;

	#ifdef _WIN32
	if (!mContextSetup)
		setupContext();

	if (mContextSetup)
	{
		std::int64_t bytesRead = 0, toRead = 5;
		std::string message(toRead, '\0');
		SecBuffer buffer[4] = {};
		SecBufferDesc descriptor = { SECBUFFER_VERSION, 4, buffer };
		SECURITY_STATUS ret;
		unsigned long fQop;

		do
		{
			if (buffer[1].BufferType == SECBUFFER_MISSING)
			{
				toRead = buffer[1].cbBuffer;
				message.resize(message.size() + toRead);
			}

			bytesRead += Socket::receive(message.data() + bytesRead, toRead, 0);

			buffer[0].pvBuffer = message.data();
			buffer[0].cbBuffer = message.size();
			buffer[0].BufferType = SECBUFFER_DATA;

			for (int i = 1; i < sizeof(buffer) / sizeof(*buffer); ++i)
			{
				buffer[i].pvBuffer = nullptr;
				buffer[i].cbBuffer = 0;
				buffer[i].BufferType = SECBUFFER_EMPTY;
			}

			ret = DecryptMessage(&mContextHandle, &descriptor, 0, &fQop);
		} while (ret == SEC_E_INCOMPLETE_MESSAGE);

		checkSSPIReturn(ret);

		result.append(static_cast<std::string::value_type*>(buffer[1].pvBuffer), buffer[1].cbBuffer);
	}
	#endif

	return result;
}

std::int64_t TLSSocket::send(void *buffer, size_t bufferSize, int flags)
{
	if (!mContextSetup)
		setupContext();

	SecBuffer secBuffer[4] = {};
	SecBufferDesc descriptor = { SECBUFFER_VERSION, 4, secBuffer };
	std::unique_ptr<std::uint8_t[]> header(new std::uint8_t[mStreamSizes.cbHeader]), trailer(new std::uint8_t[mStreamSizes.cbTrailer]);
	std::int64_t result = 0, sent = 0;

	secBuffer[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBuffer[0].pvBuffer = header.get();
	secBuffer[0].cbBuffer = mStreamSizes.cbHeader;
	secBuffer[1].BufferType = SECBUFFER_DATA;
	secBuffer[2].BufferType = SECBUFFER_STREAM_TRAILER;
	secBuffer[2].pvBuffer = trailer.get();
	secBuffer[2].cbBuffer = mStreamSizes.cbTrailer;
	secBuffer[3].BufferType = SECBUFFER_EMPTY;

	while (sent < bufferSize)
	{
		auto bytesToSend = std::min<size_t>(mStreamSizes.cbMaximumMessage, bufferSize - sent);
		std::string message(bytesToSend, '\0');

		secBuffer[1].pvBuffer = message.data();
		secBuffer[1].cbBuffer = bytesToSend;
		memcpy(message.data(), static_cast<std::uint8_t*>(buffer) + sent, bytesToSend);
		checkSSPIReturn(EncryptMessage(&mContextHandle, 0, &descriptor, 0));
		sent += bytesToSend;

		for (int i = 0; i < sizeof(secBuffer) / sizeof(*secBuffer) - 1; ++i)
			result += Socket::send(secBuffer[i].pvBuffer, secBuffer[i].cbBuffer, flags);
	}

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

NonBlockingSocket::NonBlockingSocket(Socket &socket)
	:mSocket(socket),
	oldState(mSocket.isNonBlocking())
{
	mSocket.toggleNonBlockingMode(true);
}

NonBlockingSocket::~NonBlockingSocket()
{
	mSocket.toggleNonBlockingMode(oldState);
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