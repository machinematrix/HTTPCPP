#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <string>
#include "Common.h"
#include "HttpServer.h"
#include "HttpRequest.h"
#include "RequestScheduler.h"

namespace
{
	void placeholderLogger(const std::string&)
	{

	}
}

class Http::Server::Impl
{
	enum class ServerStatus : std::uint8_t { UNINITIALIZED = 1, RUNNING, STOPPED };
	WinsockLoader mLoader;
	std::map<std::string, std::function<HandlerCallback>> mHandlers;
	std::mutex mServerMutex; //to prevent data races between the thread that created the object and the thread that serves requests
	std::thread mServerThread;
	std::condition_variable mStatusChanged;
	
	std::function<LoggerCallback> mEndpointLogger; //pointer (4|8 bytes)
	DescriptorType mSock;
	const int mQueueLength = 5;
	std::uint16_t mPort;
	std::atomic<ServerStatus> mStatus; //1 byte

	void serverProcedure();
	void handleRequest(DescriptorType) const;
public:
	Impl(std::uint16_t mPort);
	~Impl();

	void start();
	void setLogger(const std::function<LoggerCallback> &callback) noexcept;
	void setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback);
};

void Http::Server::Impl::serverProcedure()
{
	if (listen(mSock, mQueueLength) != SOCKET_ERROR) {
		mStatus.store(ServerStatus::RUNNING);
	}
	else {
		mStatus.store(ServerStatus::STOPPED);
	}

	mStatusChanged.notify_all();

	//std::unique_ptr<IRequestScheduler> scheduler(new ThreadPoolRequestScheduler(4, mSock));
	//std::unique_ptr<IRequestScheduler> scheduler(new SelectRequestScheduler(mSock));
	//std::unique_ptr<IRequestScheduler> scheduler(new PollRequestScheduler(mSock));
	std::unique_ptr<IRequestScheduler> scheduler(new RequestScheduler(mSock, 8, 5000));

	while (mStatus.load() == ServerStatus::RUNNING)
	{
		scheduler->handleRequest(std::bind(&Impl::handleRequest, this, std::placeholders::_1));
	}
}

Http::Server::Impl::~Impl()
{
	mStatus.store(ServerStatus::STOPPED);
	CloseSocket(mSock);
	if(mServerThread.joinable())
		mServerThread.join();
}

void Http::Server::Impl::handleRequest(DescriptorType clientSocket) const
{
	Request request(SocketWrapper{ clientSocket });
	
	if (request.getStatus() == Request::Status::OK)
	{
		decltype(mHandlers)::const_iterator bestMatch = mHandlers.cend();

		for (decltype(mHandlers)::const_iterator handlerSlot = mHandlers.cbegin(); handlerSlot != mHandlers.cend(); ++handlerSlot)
		{
			std::string requestResource = request.getResource();
			std::string::size_type lastSlash = requestResource.rfind('/');

			if (lastSlash != std::string::npos)
			{
				std::string::size_type matchPos = requestResource.find(handlerSlot->first);
				if (matchPos == 0
					&& !handlerSlot->first.compare(0, lastSlash + 1, requestResource, 0, lastSlash + 1)
					)
				{ //if it matches at the beginning, and to the last slash
					if(bestMatch == mHandlers.cend() || handlerSlot->first.size() > bestMatch->first.size())
						bestMatch = handlerSlot;
				}
			}
		}

		if (bestMatch != mHandlers.cend())
		{
			try {
				bestMatch->second(request);

				std::string logMessage("Served request at endpoint \"");
				logMessage += bestMatch->first;
				logMessage.push_back('\"');
				mEndpointLogger(logMessage);
			}
			catch (const std::exception&) {
				std::string msg("Unexpected exception thrown at endpoint \"");
				msg.append(bestMatch->first);
				msg.append("\" handler");
				mEndpointLogger(msg);
			}
		}

		if (request.getField(Request::HeaderField::Connection) == "keep-alive")
			return; //return without closing socket
	}

	CloseSocket(clientSocket);
}

Http::Server::Impl::Impl(std::uint16_t mPort)
	try :mSock(socket(AF_INET, SOCK_STREAM, 0)),
	mPort(mPort),
	mStatus(ServerStatus::UNINITIALIZED),
	mEndpointLogger(placeholderLogger)
{
	char optval[8] = {};
	const char *error = "Could not create server";
	std::string strPort = std::to_string(mPort);
	std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addressListPtr(nullptr, freeaddrinfo);

	if (mSock == INVALID_SOCKET)
		throw ServerException("Could not create socket");

	if (setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(optval)))
		throw ServerException(error);

	{
		addrinfo *list = NULL, hint = { 0 };
		hint.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
		hint.ai_family = AF_INET; //IPv4
		hint.ai_socktype = SOCK_STREAM;
		hint.ai_protocol = IPPROTO_TCP;
		if (getaddrinfo(nullptr, strPort.c_str(), &hint, &list))
			throw ServerException(error);
		addressListPtr.reset(list);
	}

	auto len = addressListPtr->ai_addrlen;

	if (bind(mSock, addressListPtr->ai_addr, len) == SOCKET_ERROR)
		throw ServerException(error);
}
catch (const std::runtime_error e) {
	throw ServerException(e.what());
}

void Http::Server::Impl::start()
{
	mServerThread = std::thread(&Impl::serverProcedure, this);
	std::unique_lock<std::mutex> lck(mServerMutex);

	mStatusChanged.wait(lck, [this]() -> bool { return mStatus.load() != ServerStatus::UNINITIALIZED; });

	if (mStatus.load() != ServerStatus::RUNNING) {
		throw ServerException("Could not start server");
	}
}

void Http::Server::Impl::setLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mEndpointLogger = (callback ? callback : placeholderLogger);
}

void Http::Server::Impl::setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback)
{
	mHandlers[path] = callback;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Server::Server(std::uint16_t mPort)
	:mThis(new Impl(mPort))
{}

Http::Server::~Server() noexcept = default;

Http::Server::Server(Server &&other) noexcept = default;

Http::Server& Http::Server::operator=(Server &&other) noexcept = default;

void Http::Server::start()
{
	mThis->start();
}

void Http::Server::setLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mThis->setLogger(callback);
}

void Http::Server::setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback)
{
	mThis->setResourceCallback(path, callback);
}