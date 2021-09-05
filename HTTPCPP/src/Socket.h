#ifndef __SOCKET__
#define __SOCKET__
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <variant>
#include <optional>
#include <span>

#ifdef _WIN32
#define SECURITY_WIN32
#include <ws2tcpip.h>
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
private:
	int mErrorCode = 0;
public:
	using std::runtime_error::runtime_error;
	SocketException(int code);
	int getErrorCode() const;
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
	WinsockLoader mLoader;
protected:
	DescriptorType mSocket;
private:
	int mDomain, mType, mProtocol;
	bool mNonBlocking = false;

	std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> getAddressInfo(std::string_view address, std::uint16_t port, int flags);
protected:
	//Must be used with sockets returned from accept
	Socket(DescriptorType);
public:
	Socket(int domain, int type, int protocol);
	Socket(const Socket&) = delete;
	Socket(Socket&&) noexcept;
	virtual ~Socket();
	Socket& operator=(const Socket&) = delete;
	virtual Socket& operator=(Socket&&) noexcept;

	void close();
	void bind(std::string_view address, std::uint16_t port, bool numericAddress);
	void connect(std::string_view address, std::uint16_t port, bool numericAddress);
	void listen(int queueLength);
	void toggleNonBlockingMode(bool toggle);
	bool isNonBlocking();
	void setSocketOption(int level, int optionName, const void *optionValue, int optionLength);
	virtual Socket* accept();
	virtual std::string receive(int flags = 0);
	virtual std::int64_t receive(void *buffer, size_t bufferSize, int flags = 0);
	virtual std::int64_t send(const void *buffer, size_t bufferSize, int flags = 0);
	DescriptorType get() const noexcept;
};

class TLSSocket : public Socket
{
public:
	enum class Role { CLIENT, SERVER };
private:
	std::string mCertificateStore, mCertificateSubject, mExtraData;
	std::optional<std::string> mPrincipalName;
	CredHandle mCredentialsHandle = {};
	SecHandle mContextHandle = {};
	SecPkgContext_StreamSizes mStreamSizes = {};
	Role mRole;
	bool mContextEstablished = false;

	CredHandle acquireCredentials(std::string_view certificateStore, std::string_view certificateSubject) const;
	unsigned long getContextAttributes() const noexcept;
	std::string negotiate(CredHandle&, SecHandle&, std::optional<std::span<std::byte>>);
	TLSSocket(DescriptorType, std::string_view certificateStore, std::string_view certificateSubject, Role role = Role::SERVER, const std::optional<std::string> &principalName = std::optional<std::string>());
public:
	TLSSocket(int domain, std::string_view certificateStore, std::string_view certificateSubject, Role role = Role::SERVER, const std::optional<std::string> &principalName = std::optional<std::string>());
	TLSSocket(TLSSocket&&) noexcept;
	~TLSSocket() override;
	TLSSocket& operator=(TLSSocket&&) noexcept;

	TLSSocket* accept() override;
	std::string receive(int flags = 0) override;
	//Assumes buffer is big enough to hold a full TLS message
	std::int64_t receive(void *buffer, size_t bufferSize, int flags = 0) override;
	std::int64_t send(const void *buffer, size_t bufferSize, int flags = 0) override;

	void establishSecurityContext();
	void requestRenegotiate();
	void requestRenegotiate(std::string_view certificateStore, std::string_view certificateSubject);
	std::size_t getMaxTLSMessageSize();
};

std::strong_ordering operator<=>(const Socket&, const Socket&);
bool operator==(const Socket&, const Socket&);

#endif