#include "Socket.h"
#include <algorithm>
#include <string>
#include <array>
#include <charconv>

#ifdef _WIN32
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

namespace
{
	std::string formatMessage(int code)
	{
		std::string result;

		#ifdef _WIN32
		LPSTR message;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, static_cast<DWORD>(code), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&message), 0, NULL);
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
		if (ret < 0)
			throw SocketException(ret);
	}
	#elif defined(__linux__)
	using BufferType = void*;
	using LengthType = size_t;
	#endif
}

SocketException::SocketException(int code)
	:std::runtime_error(formatMessage(code)),
	mErrorCode(code)
{}

int SocketException::getErrorCode() const
{
	return mErrorCode;
}

std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> Socket::getAddressInfo(std::string_view address, std::uint16_t port, int flags)
{
	std::array<char, 6> strPort = {}; //Long enough for a 16 bit integer, plus a null character.
	std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> result(nullptr, freeaddrinfo);

	addrinfo *list = nullptr, hint = { 0 };
	hint.ai_flags = flags;
	hint.ai_family = mDomain; //IPv4
	hint.ai_socktype = mType;
	hint.ai_protocol = mProtocol;
	std::to_chars(strPort.data(), strPort.data() + strPort.size(), port);
	auto returnValue = getaddrinfo(address.data(), strPort.data(), &hint, &list);
	result.reset(list); //Take ownership of the pointer
	list = nullptr;

	if (returnValue)
		throw SocketException(returnValue);

	return result;
}

Socket::Socket(DescriptorType sock)
	:mSocket(sock),
	mDomain(0),
	mType(0),
	mProtocol(0)
{
	#ifdef	_WIN32
	using StructLength = int;
	#elif defined __linux__
	using StructLength = socklen_t;
	#endif

	struct
	{
		CSADDR_INFO info;
		sockaddr dummies[2];
	} state;
	WSAPROTOCOL_INFOW protocolInfo;
	StructLength typeLen = sizeof(socklen_t), stateLength = sizeof(state), protocolInfoLength = sizeof(protocolInfo);

	try
	{
		checkReturn(getsockopt(mSocket, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&protocolInfo), &protocolInfoLength));
		checkReturn(getsockopt(mSocket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&mType), &typeLen));
		//This actually requires a buffer to hold a CSADDR_INFO plus two sockaddr structures, which will be pointed to by the lpSockaddr members of the LocalAddr and RemoteAddr members of the CSADDR_INFO structure
		//more info: https://stackoverflow.com/questions/65782944/socket-option-so-bsp-state-fails-with-wsaefault
		checkReturn(getsockopt(mSocket, SOL_SOCKET, SO_BSP_STATE, reinterpret_cast<char*>(&state), &stateLength));
		mDomain = protocolInfo.iAddressFamily; //Could be retrieved with SO_DOMAIN option on linux.
		mProtocol = state.info.iProtocol;
	}
	catch (const SocketException&)
	{
		close();
		throw;
	}
}

Socket::Socket(int domain, int type, int protocol)
	:mSocket(socket(domain, type, protocol)),
	mDomain(domain),
	mType(type),
	mProtocol(protocol)
{
	if (mSocket == INVALID_SOCKET)
		#ifdef _WIN32
		throw SocketException(WSAGetLastError());
		#elif defined (__linux__)
		throw SocketException(errno);
		#endif
}

Socket::Socket(Socket &&other) noexcept
	:mSocket(other.mSocket),
	mLoader(std::move(other.mLoader)),
	mDomain(other.mDomain),
	mType(other.mType),
	mProtocol(other.mProtocol)
{
	other.mSocket = INVALID_SOCKET;
}

Socket::~Socket()
{
	close();
}

Socket& Socket::operator=(Socket &&other) noexcept
{
	mSocket = other.mSocket;
	mLoader = std::move(other.mLoader);
	other.mSocket = INVALID_SOCKET;
	mDomain = other.mDomain;
	mType = other.mType;
	mProtocol = other.mProtocol;

	return *this;
}

void Socket::close()
{
	#ifdef _WIN32
	closesocket(mSocket);
	#elif defined (__linux__)
	::close(mSocket);
	#endif
}

void Socket::bind(std::string_view address, std::uint16_t port, bool numericAddress)
{
	auto addressListPointer = getAddressInfo(address, port, (numericAddress ? AI_NUMERICSERV | AI_PASSIVE : AI_PASSIVE));

	checkReturn(::bind(mSocket, addressListPointer->ai_addr, static_cast<int>(addressListPointer->ai_addrlen)));
}

void Socket::connect(std::string_view address, std::uint16_t port, bool numericAddress)
{
	auto addressListPointer = getAddressInfo(address, port, (numericAddress ? AI_NUMERICSERV : 0));

	checkReturn(::connect(mSocket, addressListPointer->ai_addr, static_cast<int>(addressListPointer->ai_addrlen)));
}

void Socket::listen(int queueLength)
{
	checkReturn(::listen(mSocket, queueLength));
}

void Socket::toggleNonBlockingMode(bool toggle)
{
	#ifdef _WIN32
	u_long toggleLong = mNonBlocking = toggle;
	checkReturn(ioctlsocket(mSocket, FIONBIO, &toggleLong));
	#elif defined (__linux__)
	int flags = fcntl(mSocket, F_GETFL, 0);
	checkReturn(flags);
	flags = toggle ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
	checkReturn(fcntl(mSocket, F_SETFL, flags));
	#endif
}

bool Socket::isNonBlocking()
{
	#ifdef _WIN32
	return mNonBlocking;
	#elif defined (__linux__)
	int flags = fcntl(mSocket, F_GETFL, 0);
	checkReturn(flags);
	return flags & O_NONBLOCK;
	#endif
}

void Socket::setSocketOption(int level, int optionName, const void *optionValue, int optionLength)
{
	#ifdef _WIN32
	checkReturn(setsockopt(mSocket, level, optionName, static_cast<const char*>(optionValue), optionLength));
	#elif defined (__linux__)
	checkReturn(setsockopt(mSocket, level, optionName, optionValue, static_cast<socklen_t>(optionLength)));
	#endif
}

Socket* Socket::accept()
{
	DescriptorType clientSocket = ::accept(mSocket, nullptr, nullptr);

	if (clientSocket != INVALID_SOCKET)
		return new Socket(clientSocket);
	else
		#ifdef _WIN32
		throw SocketException(WSAGetLastError());
		#elif defined (__linux__)
		throw SocketException(errno);
		#endif
}

std::string Socket::receive(int flags)
{
	std::string buffer(1024, '\0');

	buffer.resize(receive(buffer.data(), buffer.size(), flags));

	return buffer;
}

std::int64_t Socket::receive(void *buffer, size_t bufferSize, int flags)
{
	std::int64_t result = recv(mSocket, static_cast<BufferType>(buffer), static_cast<LengthType>(bufferSize), flags);

	if (bufferSize && !result)
		throw SocketException("The other side closed the connection (recv returned 0)");
	checkReturn(static_cast<int>(result));

	return result;
}

std::int64_t Socket::send(const void *buffer, size_t bufferSize, int flags)
{
	std::int64_t result = ::send(mSocket, static_cast<const char*>(buffer), static_cast<LengthType>(bufferSize), flags);

	checkReturn(static_cast<int>(result));

	return result;
}

DescriptorType Socket::get() const noexcept
{
	return mSocket;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

CredHandle TLSSocket::acquireCredentials(std::string_view certificateStore, std::string_view certificateSubject) const
{
	using std::unique_ptr;
	using std::remove_pointer;
	unique_ptr<remove_pointer<HCERTSTORE>::type, std::function<BOOL __stdcall(HCERTSTORE)>> certificateStorePointer(nullptr, std::bind(CertCloseStore, std::placeholders::_1, CERT_CLOSE_STORE_FORCE_FLAG));
	unique_ptr<const CERT_CONTEXT, decltype(CertFreeCertificateContext) *> certificate(nullptr, CertFreeCertificateContext);
	CredHandle credentialsHandle = {};

	certificateStorePointer.reset(CertOpenSystemStoreA(NULL, certificateStore.data())/*CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"MY")*/);
	if (!certificateStorePointer)
		throw SocketException(GetLastError());

	certificate.reset(CertFindCertificateInStore(certificateStorePointer.get(), X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_A, certificateSubject.data(), NULL));
	if (!certificate)
		throw SocketException(GetLastError());

	auto certPtr = certificate.get();
	SCHANNEL_CRED schannelCredential = { .dwVersion = SCHANNEL_CRED_VERSION, .cCreds = 1, .paCred = &certPtr };

	checkSSPIReturn(AcquireCredentialsHandleA(nullptr, const_cast<char *>("Schannel"), mRole == Role::SERVER ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, nullptr, &schannelCredential, nullptr, nullptr, &credentialsHandle, nullptr));

	return credentialsHandle;
}

unsigned long TLSSocket::getContextAttributes() const noexcept
{
	unsigned long result;

	switch (mRole)
	{
		case TLSSocket::Role::SERVER:
			result = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM;
			break;
		case TLSSocket::Role::CLIENT:
			result = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_STREAM;
			if (!mPrincipalName.has_value())
				result |= ISC_REQ_MANUAL_CRED_VALIDATION;
			break;
		default:
			result = 0;
			break;
	}

	return result;
}

std::string TLSSocket::negotiate(CredHandle &credentialsHandle, SecHandle &contextHandle, std::optional<std::span<std::byte>> initialBuffer = std::optional<std::span<std::byte>> {})
{
	//certmgr.msc - certificate store
	/*unsigned long packageCount;
	PSecPkgInfoA packages;
	EnumerateSecurityPackagesA(&packageCount, &packages);

	for (decltype(packageCount) i = 0; i < packageCount; ++i)
		std::cout << packages[i].Name << std::endl;*/
	PSecPkgInfoA packageInfo;
	constexpr decltype(packageInfo->cbMaxToken) initialBytesToRead = 16;
	decltype(packageInfo->cbMaxToken) maxMessage, bytesToRead = initialBytesToRead;
	ULONG returnedAttributes {};

	checkSSPIReturn(QuerySecurityPackageInfoA(const_cast<char*>("Schannel"), &packageInfo));
	maxMessage = packageInfo->cbMaxToken;
	FreeContextBuffer(packageInfo);

	std::string extraData;
	std::unique_ptr<std::byte[]> inputBufferMemory { std::make_unique<std::byte[]>(maxMessage) };
	std::unique_ptr<std::byte[]> outputBufferMemory { std::make_unique<std::byte[]>(maxMessage) };
	std::int64_t bytesRead = 0;
	SECURITY_STATUS result {};

	do
	{
		SecBuffer inputBuffer[2] = { { .BufferType = SECBUFFER_TOKEN }, { .cbBuffer = 0, .BufferType = SECBUFFER_EMPTY, .pvBuffer = nullptr } };
		SecBuffer outputBuffer = { .cbBuffer = maxMessage, .BufferType = SECBUFFER_TOKEN, .pvBuffer = outputBufferMemory.get() };
		SecBufferDesc inputBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = inputBuffer };
		SecBufferDesc outputBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 1, .pBuffers = &outputBuffer };

		if (initialBuffer.has_value()) //process data sent by the caller first
		{
			inputBuffer[0].pvBuffer = initialBuffer.value().data();
			inputBuffer[0].cbBuffer = initialBuffer.value().size();
			initialBuffer = decltype(initialBuffer) {};
		}
		else if (mRole == Role::SERVER || contextHandle.dwLower || contextHandle.dwUpper)
		{
			inputBuffer[0].pvBuffer = inputBufferMemory.get();
			inputBuffer[0].cbBuffer = bytesRead += Socket::receive(inputBufferMemory.get() + bytesRead, bytesToRead, 0);
		}

		if (mRole == Role::SERVER)
			result = AcceptSecurityContext(&credentialsHandle, contextHandle.dwLower || contextHandle.dwUpper ? &contextHandle : nullptr, &inputBufferDescriptor, getContextAttributes(), 0, &contextHandle, &outputBufferDescriptor, &returnedAttributes, nullptr);
		else if (mRole == Role::CLIENT)
			result = InitializeSecurityContextA(&credentialsHandle, contextHandle.dwLower || contextHandle.dwUpper ? &contextHandle : nullptr, mPrincipalName ? mPrincipalName.value().data() : nullptr, getContextAttributes(), 0, 0, contextHandle.dwLower || contextHandle.dwUpper ? &inputBufferDescriptor : nullptr, 0, &contextHandle, &outputBufferDescriptor, &returnedAttributes, nullptr);
		
		switch (result)
		{
			case SEC_I_COMPLETE_NEEDED:
			case SEC_I_COMPLETE_AND_CONTINUE:
				checkSSPIReturn(CompleteAuthToken(&contextHandle, &outputBufferDescriptor));
				[[fallthrough]];
			case SEC_E_OK:
				if (outputBuffer.BufferType == SECBUFFER_EXTRA)
					extraData.append(static_cast<std::string::value_type*>(inputBuffer[0].pvBuffer) + inputBuffer[0].cbBuffer - inputBuffer[1].cbBuffer, inputBuffer[1].cbBuffer);
				[[fallthrough]];
			case SEC_I_CONTINUE_NEEDED:
			{
				if (inputBuffer[1].BufferType == SECBUFFER_EXTRA)
				{
					//https://docs.microsoft.com/en-us/windows/win32/secauthn/extra-buffers-returned-by-schannel
					std::byte *extraBytesBegin = static_cast<std::byte *>(inputBuffer[0].pvBuffer) + inputBuffer[0].cbBuffer - inputBuffer[1].cbBuffer;
					bytesRead = inputBuffer[1].cbBuffer;
					bytesToRead = 0; //Do not read data in the next iteration, just process the extra data read
					std::copy(extraBytesBegin, extraBytesBegin + bytesRead, inputBufferMemory.get());
				}
				else
				{
					bytesToRead = initialBytesToRead;
					bytesRead = 0;
				}
				
				if (outputBuffer.BufferType == SECBUFFER_TOKEN)
				{
					std::int64_t bytesSent = 0;
					while ((bytesSent += Socket::send(outputBuffer.pvBuffer, outputBuffer.cbBuffer - bytesSent, 0)) < outputBuffer.cbBuffer);
				}

				break;
			}
			case SEC_E_INCOMPLETE_MESSAGE:
				if (inputBuffer[1].BufferType == SECBUFFER_MISSING)
					bytesToRead = inputBuffer[1].cbBuffer;
				break;
			case SEC_I_NO_RENEGOTIATION:
			case SEC_I_CONTEXT_EXPIRED:
				throw SocketException { result };
				break;
			default:
				checkSSPIReturn(result);
				bytesToRead = initialBytesToRead;
				break;
		}
	}
	while (result == SEC_I_CONTINUE_NEEDED || result == SEC_I_COMPLETE_AND_CONTINUE || result == SEC_E_INCOMPLETE_MESSAGE);

	//SecPkgContext_Sizes sizes;
	//SecPkgContext_ConnectionInfo connInfo;
	if (returnedAttributes != getContextAttributes())
		throw SocketException("The established context does not satisfy the requested attributes");
	checkSSPIReturn(QueryContextAttributes(&contextHandle, SECPKG_ATTR_STREAM_SIZES, &mStreamSizes));
	//checkSSPIReturn(QueryContextAttributes(&contextHandle, SECPKG_ATTR_SIZES, &sizes));
	//checkSSPIReturn(QueryContextAttributes(&contextHandle, SECPKG_ATTR_CONNECTION_INFO, &connInfo));

	return extraData;
}

TLSSocket::TLSSocket(DescriptorType sock, std::string_view certificateStore, std::string_view certificateSubject, Role role, const std::optional<std::string> &principalName)
	:Socket(sock),
	mCertificateStore(certificateStore),
	mCertificateSubject(certificateSubject),
	mRole(role),
	mPrincipalName(principalName)
{}

TLSSocket::TLSSocket(int domain, std::string_view certificateStore, std::string_view certificateSubject, Role role, const std::optional<std::string> &principalName)
	:Socket(domain, SOCK_STREAM, IPPROTO_TCP),
	mCertificateStore(certificateStore),
	mCertificateSubject(certificateSubject),
	mRole(role),
	mPrincipalName(principalName)
{}

TLSSocket::TLSSocket(TLSSocket &&other) noexcept
	:Socket(std::move(other)),
	mCertificateStore(std::move(other.mCertificateStore)),
	mCertificateSubject(std::move(other.mCertificateSubject)),
	mExtraData(std::move(other.mExtraData)),
	mCredentialsHandle(std::exchange(other.mCredentialsHandle, CredHandle {})),
	mContextHandle(std::exchange(other.mContextHandle, SecHandle {})),
	mStreamSizes(other.mStreamSizes),
	mContextEstablished(other.mContextEstablished),
	mRole(other.mRole),
	mPrincipalName(other.mPrincipalName)
{}

TLSSocket::~TLSSocket()
{
	DeleteSecurityContext(&mContextHandle);
	FreeCredentialsHandle(&mCredentialsHandle);
}

TLSSocket& TLSSocket::operator=(TLSSocket &&other) noexcept
{
	*static_cast<Socket*>(this) = std::move(other);
	mCertificateStore = std::move(other.mCertificateStore);
	mCertificateSubject = std::move(other.mCertificateSubject);
	mExtraData = std::move(other.mExtraData);
	mCredentialsHandle = std::exchange(other.mCredentialsHandle, CredHandle {});
	mContextHandle = std::exchange(other.mContextHandle, SecHandle {});
	mStreamSizes = other.mStreamSizes;
	mContextEstablished = other.mContextEstablished;
	mRole = other.mRole;
	mPrincipalName = std::move(other.mPrincipalName);

	return *this;
}

TLSSocket* TLSSocket::accept()
{
	DescriptorType clientSocket = ::accept(mSocket, nullptr, nullptr);

	if (clientSocket != INVALID_SOCKET)
		return new TLSSocket(clientSocket, mCertificateStore, mCertificateSubject, Role::SERVER);
	else
		#ifdef _WIN32
		throw SocketException(WSAGetLastError());
	#elif defined (__linux__)
		throw SocketException(errno);
	#endif
}

std::string TLSSocket::receive(int flags)
{
	if (!mContextEstablished)
		establishSecurityContext();

	std::string result, message(mExtraData);
	SecBuffer buffer[4] = {};
	SecBufferDesc descriptor = { SECBUFFER_VERSION, 4, buffer };
	std::int64_t bytesRead = mExtraData.size(), toRead = mStreamSizes.cbHeader;
	SECURITY_STATUS returnValue;

	message.resize(message.size() + toRead);
	mExtraData.clear();

	do
	{
		if (buffer[1].BufferType == SECBUFFER_MISSING)
		{
			toRead = buffer[1].cbBuffer;
			message.resize(message.size() + toRead);
		}

		std::int64_t read = Socket::receive(message.data() + bytesRead, toRead, 0);

		bytesRead += read;
		buffer[0].pvBuffer = message.data();
		buffer[0].cbBuffer = message.size() - (toRead - read); //this used to be just message.size(), which assumes that I always receive toRead bytes, which is not always the case and would sometimes cause decrypt errors.
		buffer[0].BufferType = SECBUFFER_DATA;

		std::fill(std::begin(buffer) + 1, std::end(buffer), SecBuffer { .cbBuffer = 0, .BufferType = SECBUFFER_EMPTY, .pvBuffer = nullptr });

		returnValue = DecryptMessage(&mContextHandle, &descriptor, 0, nullptr);

		if (returnValue == SEC_I_RENEGOTIATE)
		{
			message = negotiate(mCredentialsHandle, mContextHandle, std::span (static_cast<std::byte *>(buffer[0].pvBuffer), bytesRead));
			bytesRead = message.size(); //start again
			toRead = mStreamSizes.cbHeader;
			message.resize(message.size() + mStreamSizes.cbHeader);
			returnValue = SEC_E_INCOMPLETE_MESSAGE; //so the loop is not exited
		}
		else if (returnValue == SEC_I_CONTEXT_EXPIRED)
		{
			DWORD shutdown = SCHANNEL_SHUTDOWN;
			SecBuffer shutdownBuffer = { .cbBuffer = sizeof(shutdown), .BufferType = SECBUFFER_TOKEN, .pvBuffer = &shutdown };
			SecBufferDesc shutdownBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 1, .pBuffers = &shutdownBuffer };

			checkSSPIReturn(ApplyControlToken(&mContextHandle, &shutdownBufferDescriptor));
			negotiate(mCredentialsHandle, mContextHandle, std::span(static_cast<std::byte *>(buffer[0].pvBuffer), bytesRead));
			throw SocketException { SEC_I_CONTEXT_EXPIRED };
		}
	} while (returnValue == SEC_E_INCOMPLETE_MESSAGE);

	checkSSPIReturn(returnValue);

	result.append(static_cast<std::string::value_type*>(buffer[1].pvBuffer), buffer[1].cbBuffer);

	return result;
}

std::int64_t TLSSocket::receive(void *buffer, size_t, int flags)
{
	using std::byte;

	if (!mContextEstablished)
		establishSecurityContext();

	SecBuffer messageBuffer[4] = {};
	SecBufferDesc messageBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 4, .pBuffers = messageBuffer };
	std::int64_t bytesRead = mExtraData.size(), bytesToRead = mStreamSizes.cbHeader;
	SECURITY_STATUS returnValue;

	std::copy(mExtraData.begin(), mExtraData.end(), static_cast<std::string::value_type*>(buffer));
	mExtraData.clear();

	do
	{
		if (messageBuffer[1].BufferType == SECBUFFER_MISSING)
			bytesToRead = messageBuffer[1].cbBuffer;

		bytesRead += Socket::receive(static_cast<byte *>(buffer) + bytesRead, bytesToRead, 0);
		messageBuffer[0].pvBuffer = buffer;
		messageBuffer[0].cbBuffer = bytesRead; //this used to be just message.size(), which assumes that I always receive toRead bytes, which is not always the case and would sometimes cause decrypt errors.
		messageBuffer[0].BufferType = SECBUFFER_DATA;

		for (std::size_t i = 1; i < sizeof(messageBuffer) / sizeof(*messageBuffer); ++i)
		{
			messageBuffer[i].pvBuffer = nullptr;
			messageBuffer[i].cbBuffer = 0;
			messageBuffer[i].BufferType = SECBUFFER_EMPTY;
		}

		returnValue = DecryptMessage(&mContextHandle, &messageBufferDescriptor, 0, nullptr);

		if (returnValue == SEC_I_RENEGOTIATE)
		{
			auto extraData = negotiate(mCredentialsHandle, mContextHandle, std::span(static_cast<std::byte *>(buffer), bytesRead));
			std::copy(extraData.begin(), extraData.end(), static_cast<std::string::value_type*>(buffer));
			bytesRead = extraData.size(); //start again
			bytesToRead = mStreamSizes.cbHeader;
			returnValue = SEC_E_INCOMPLETE_MESSAGE; //so the loop is not exited
		}
		else if (returnValue == SEC_I_CONTEXT_EXPIRED)
		{
			DWORD shutdown = SCHANNEL_SHUTDOWN;
			SecBuffer shutdownBuffer = { .cbBuffer = sizeof(shutdown), .BufferType = SECBUFFER_TOKEN, .pvBuffer = &shutdown };
			SecBufferDesc shutdownBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 1, .pBuffers = &shutdownBuffer };

			checkSSPIReturn(ApplyControlToken(&mContextHandle, &shutdownBufferDescriptor));
			negotiate(mCredentialsHandle, mContextHandle, std::span(static_cast<std::byte *>(messageBuffer[0].pvBuffer), bytesRead));
			throw SocketException { SEC_I_CONTEXT_EXPIRED };
		}
	} while (returnValue == SEC_E_INCOMPLETE_MESSAGE);

	checkSSPIReturn(returnValue);

	for (decltype(SecBuffer::cbBuffer) i = 0; i < messageBuffer[1].cbBuffer; ++i) //Move the bytes of the decrypted message to the beginning of the buffer
		static_cast<byte *>(messageBuffer[0].pvBuffer)[i] = static_cast<byte *>(messageBuffer[1].pvBuffer)[i];

	return messageBuffer[1].cbBuffer;
}

std::int64_t TLSSocket::send(const void *buffer, size_t bufferSize, int flags)
{
	if (!mContextEstablished)
		establishSecurityContext();

	SecBuffer secBuffer[4];
	SecBufferDesc descriptor = { SECBUFFER_VERSION, 4, secBuffer };
	std::unique_ptr<std::byte[]> header(std::make_unique<std::byte[]>(mStreamSizes.cbHeader)), trailer(std::make_unique<std::byte[]>(mStreamSizes.cbTrailer));
	std::int64_t sent = 0;

	secBuffer[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBuffer[0].pvBuffer = header.get();
	secBuffer[0].cbBuffer = mStreamSizes.cbHeader;
	secBuffer[1].BufferType = SECBUFFER_DATA;
	secBuffer[2].BufferType = SECBUFFER_STREAM_TRAILER;
	secBuffer[2].pvBuffer = trailer.get();
	secBuffer[2].cbBuffer = mStreamSizes.cbTrailer;
	secBuffer[3].BufferType = SECBUFFER_EMPTY;
	secBuffer[3].pvBuffer = nullptr;
	secBuffer[3].cbBuffer = 0;

	while (sent < bufferSize)
	{
		auto bytesToSend = std::min<size_t>(mStreamSizes.cbMaximumMessage, bufferSize - sent);
		auto message { std::make_unique<std::byte[]>(bytesToSend) };

		secBuffer[1].pvBuffer = message.get();
		secBuffer[1].cbBuffer = bytesToSend;
		std::copy(static_cast<const std::byte *>(buffer) + sent, static_cast<const std::byte *>(buffer) + sent + bytesToSend, message.get());
		checkSSPIReturn(EncryptMessage(&mContextHandle, 0, &descriptor, 0));
		sent += bytesToSend;

		for (int i = 0; i < sizeof(secBuffer) / sizeof(*secBuffer) - 1; ++i)
			Socket::send(secBuffer[i].pvBuffer, secBuffer[i].cbBuffer, flags);
	}

	return bufferSize;
}

void TLSSocket::establishSecurityContext()
{
	using std::remove_pointer;
	using std::unique_ptr;
	if (mContextEstablished)
		return;

	CredHandle credentialsHandle { acquireCredentials(mCertificateStore, mCertificateSubject) };
	SecHandle contextHandle = {};

	try
	{
		mExtraData = negotiate(credentialsHandle, contextHandle);
		mCredentialsHandle = credentialsHandle;
		mContextHandle = contextHandle;
		mContextEstablished = true;
	}
	catch (const SocketException&)
	{
		mExtraData.clear();
		DeleteSecurityContext(&contextHandle);
		FreeCredentialsHandle(&credentialsHandle);
		throw;
	}
}

//https://docs.microsoft.com/en-us/windows/win32/secauthn/renegotiating-an-schannel-connection
void TLSSocket::requestRenegotiate()
{
	if (!mContextEstablished)
		throw SocketException("A security context must be established before a new handshake can be performed");

	PSecPkgInfoA packageInfo;
	checkSSPIReturn(QuerySecurityPackageInfoA(const_cast<char *>("Schannel"), &packageInfo));
	auto maxToken = packageInfo->cbMaxToken;
	FreeContextBuffer(packageInfo);

	auto alertBufferMemory { std::make_unique<std::byte[]>(maxToken) }, outputBufferMemory { std::make_unique<std::byte[]>(maxToken) };
	SecBuffer outputBuffer[2] = {
		{ .cbBuffer = maxToken, .BufferType = SECBUFFER_TOKEN, .pvBuffer = outputBufferMemory.get() },
		{ .cbBuffer = maxToken, .BufferType = SECBUFFER_ALERT, .pvBuffer = alertBufferMemory.get() }
	};
	SecBufferDesc outputBufferDescriptor { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = outputBuffer };
	SECURITY_STATUS returnValue {};
	ULONG returnedAttributes = 0;

	if (mRole == Role::SERVER)
		returnValue = AcceptSecurityContext(&mCredentialsHandle, &mContextHandle, nullptr, getContextAttributes(), 0, nullptr, &outputBufferDescriptor, &returnedAttributes, nullptr);
	else if (mRole == Role::CLIENT)
		returnValue = InitializeSecurityContextA(&mCredentialsHandle, &mContextHandle, mPrincipalName ? mPrincipalName.value().data() : nullptr, getContextAttributes(), 0, 0, nullptr, 0, nullptr, &outputBufferDescriptor, &returnedAttributes, nullptr);

	std::string_view alert { static_cast<char*>(outputBuffer[1].pvBuffer), outputBuffer[1].cbBuffer };

	checkSSPIReturn(returnValue);
	Socket::send(outputBuffer[0].pvBuffer, outputBuffer[0].cbBuffer);
	mExtraData = negotiate(mCredentialsHandle, mContextHandle);
}

void TLSSocket::requestRenegotiate(std::string_view certificateStore, std::string_view certificateSubject)
{
	auto newCredentials { acquireCredentials(certificateStore, certificateSubject) };
	auto oldCredentials = mCredentialsHandle;

	mCredentialsHandle = newCredentials;
	try
	{
		requestRenegotiate();
		FreeCredentialsHandle(&oldCredentials); //If the renegotiation succeeded, free the old credentials
	}
	catch (const SocketException&)
	{
		mCredentialsHandle = oldCredentials;
		FreeCredentialsHandle(&newCredentials); //If the renegotiation failed, free the new credentials
		throw;
	}
}

std::size_t TLSSocket::getMaxTLSMessageSize()
{
	if (mContextEstablished)
		return static_cast<std::size_t>(mStreamSizes.cbHeader) + mStreamSizes.cbMaximumMessage + mStreamSizes.cbTrailer;
	else
		throw SocketException("A security context must be established before this method can be called");
}

//https://docs.microsoft.com/en-us/windows/win32/secauthn/shutting-down-an-schannel-connection
void TLSSocket::shutdownConnection()
{
	DWORD shutdownToken = SCHANNEL_SHUTDOWN;
	SecBuffer shutdownBuffer = { .cbBuffer = sizeof(shutdownToken), .BufferType = SECBUFFER_TOKEN, .pvBuffer = &shutdownToken };
	SecBufferDesc shutdownBufferDescriptor = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 1, .pBuffers = &shutdownBuffer };

	if (!mContextEstablished)
		throw SocketException("A security context must be established before it can be shut down");

	checkSSPIReturn(ApplyControlToken(&mContextHandle, &shutdownBufferDescriptor));
	try
	{
		requestRenegotiate();
	}
	catch (const SocketException &e)
	{
		if (e.getErrorCode() != SEC_I_CONTEXT_EXPIRED)
			throw;
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

std::strong_ordering operator<=>(const Socket &lhs, const Socket &rhs)
{
	return lhs.get() <=> rhs.get();
}

bool operator==(const Socket &lhs, const Socket &rhs)
{
	return lhs.get() == rhs.get();
}