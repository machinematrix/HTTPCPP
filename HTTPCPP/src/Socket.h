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

class WinsockLoader;

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

class Socket
{
	friend class SocketPoller;
	friend bool operator!=(const Socket&, const Socket&) noexcept;
	friend bool operator<(const Socket&, const Socket&) noexcept;
	std::unique_ptr<WinsockLoader> loader;
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
	void bind(std::string_view address, short port, bool numericAddress);
	void listen(int queueLength);
	void toggleNonBlockingMode(bool toggle);
	bool isNonBlocking();
	void setSocketOption(int level, int optionName, const void *optionValue, int optionLength);
	virtual Socket* accept();
	virtual std::string receive(int flags);
	virtual std::int64_t receive(void *buffer, size_t bufferSize, int flags);
	virtual std::int64_t send(void *buffer, size_t bufferSize, int flags);
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

class NonBlockingSocket
{
	Socket &mSocket;
	bool oldState;
public:
	NonBlockingSocket(Socket&);
	~NonBlockingSocket();
};

class SocketPoller
{
	std::vector<std::shared_ptr<Socket>> mSockets;
	std::vector<PollFileDescriptor> mPollFdList;
public:
	SocketPoller() = default;
	SocketPoller(const SocketPoller&) = delete;
	SocketPoller(SocketPoller&&) noexcept;

	SocketPoller& operator=(const SocketPoller&) = delete;
	SocketPoller& operator=(SocketPoller&&) noexcept;

	void addSocket(decltype(mSockets)::value_type socket, decltype(PollFileDescriptor::events) events);
	void removeSocket(const decltype(mSockets)::value_type::element_type &socket);
	void poll(int timeout, std::function<void(decltype(mSockets)::value_type, decltype(PollFileDescriptor::revents))> callback);
};

#endif