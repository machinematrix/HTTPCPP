#ifndef __SOCKET__
#define __SOCKET__
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <variant>

#ifdef _WIN32
#define SECURITY_WIN32
#include <winsock2.h>
#include <security.h>
using PollFileDescriptor = WSAPOLLFD;
using DescriptorType = SOCKET;
#elif defined __linux__
#include <poll.h>
using PollFileDescriptor = pollfd;
using DescriptorType = int;
#endif

class SocketException : public std::runtime_error
{
public:
	using AdditionalInformationType = std::variant<decltype(SecBuffer::cbBuffer)>;
private:
	int mErrorCode;
	AdditionalInformationType mAdditionalInformation;
public:
	using std::runtime_error::runtime_error;
	SocketException(int code);
	int getErrorCode() const;
	AdditionalInformationType getAdditionalInformation() const;
	void setAdditionalInformation(const AdditionalInformationType &info);
};

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

	WinsockLoader& operator=(const WinsockLoader &)
	{
		startup();
		return *this;
	}

	WinsockLoader& operator=(WinsockLoader &&) noexcept = default;
};

class Socket
{
	friend class SocketPoller;
	friend bool operator!=(const Socket&, const Socket&) noexcept;
	friend bool operator<(const Socket&, const Socket&) noexcept;
	WinsockLoader mLoader;
protected:
	DescriptorType mSock;
private:
	int mDomain, mType, mProtocol;
	bool mNonBlocking = false;
public:
	//Must be used with sockets returned from accept
	Socket(DescriptorType);
	Socket(int domain, int type, int protocol);
	Socket(const Socket&) = delete;
	Socket(Socket&&) noexcept;
	virtual ~Socket();
	Socket& operator=(const Socket&) = delete;
	virtual Socket& operator=(Socket&&) noexcept;

	void close();
	void bind(std::string_view address, std::uint16_t port, bool numericAddress);
	void listen(int queueLength);
	void toggleNonBlockingMode(bool toggle);
	bool isNonBlocking();
	void setSocketOption(int level, int optionName, const void *optionValue, int optionLength);
	virtual Socket* accept();
	virtual std::string receive(int flags);
	virtual std::int64_t receive(void *buffer, size_t bufferSize, int flags);
	virtual std::int64_t send(void *buffer, size_t bufferSize, int flags);
	DescriptorType get();
};

class TLSSocket : public Socket
{
	std::string mCertificateStore, mCertificateName;
	CredHandle mCredentialsHandle = {};
	SecHandle mContextHandle = {};
	SecPkgContext_StreamSizes mStreamSizes = {};
	bool mNegotiationCompleted = false;

	std::string negotiate();
public:
	TLSSocket(DescriptorType, std::string_view certificateStore, std::string_view certificateName);
	TLSSocket(int domain, int type, int protocol, std::string_view certificateStore, std::string_view certificateName);
	TLSSocket(TLSSocket&&) noexcept;
	~TLSSocket() override;
	TLSSocket& operator=(TLSSocket&&) noexcept;

	TLSSocket* accept() override;
	std::string receive(int flags);
	std::int64_t receive(void *buffer, size_t bufferSize, int flags) override;
	std::int64_t send(void *buffer, size_t bufferSize, int flags) override;
};

bool operator!=(const Socket&, const Socket&) noexcept;
bool operator==(const Socket&, const Socket&) noexcept;
bool operator<(const Socket&, const Socket&) noexcept;

#endif