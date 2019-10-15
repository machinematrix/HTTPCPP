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
	void placeholderLogger(const std::string_view&)
	{

	}
}

class Http::HttpServer::Impl
{
	WinsockLoader mLoader;
	std::map<std::string, std::function<HandlerCallback>> mHandlers;
	std::mutex mServerMutex; //to prevent data races between the thread that created the object and the thread that serves requests
	std::thread mServerThread;
	std::condition_variable mStatusChanged;
	
	std::function<LoggerCallback> mLogger; //pointer (4|8 bytes)
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

void Http::HttpServer::Impl::serverProcedure()
{
	if (listen(mSock, mQueueLength) != SOCKET_ERROR) {
		mStatus.store(ServerStatus::RUNNING);
	}
	else {
		mStatus.store(ServerStatus::STOPPED);
	}

	mStatusChanged.notify_all();

	std::unique_ptr<IRequestScheduler> scheduler(new ThreadPoolRequestScheduler(4, mSock));
	//std::unique_ptr<IRequestScheduler> scheduler(new SelectRequestScheduler(mSock));
	//std::unique_ptr<IRequestScheduler> scheduler(new PollRequestScheduler(mSock));

	while (mStatus.load() == ServerStatus::RUNNING)
	{
		scheduler->handleRequest(std::bind(&Impl::handleRequest, this, std::placeholders::_1));
	}
}

Http::HttpServer::Impl::~Impl()
{
	mStatus.store(ServerStatus::STOPPED);
	CloseSocket(mSock);
	mServerThread.join();
}

void Http::HttpServer::Impl::handleRequest(DescriptorType clientSocket) const
{
	HttpRequest request(SocketWrapper{ clientSocket });
	
	if (request.getStatus() == HttpRequest::Status::OK)
	{
		decltype(mHandlers)::const_iterator bestMatch = mHandlers.cend();

		for (decltype(mHandlers)::const_iterator handlerSlot = mHandlers.cbegin(); handlerSlot != mHandlers.cend(); ++handlerSlot)
		{
			std::string_view requestResource = request.getResource();
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
				mLogger(logMessage);
			}
			catch (const std::exception&) {
				std::string msg("Unexpected exception thrown at endpoint \"");
				msg.append(bestMatch->first);
				msg.append("\" handler");
				mLogger(msg);
			}
		}
	}

	CloseSocket(clientSocket);
}

Http::HttpServer::Impl::Impl(std::uint16_t mPort)
	try :mSock(socket(AF_INET, SOCK_STREAM, 0)),
	mPort(mPort),
	mStatus(ServerStatus::UNINITIALIZED),
	mLogger(placeholderLogger)
{
	char optval[8] = {};
	const char *error = "Could not create server";
	std::string strPort = std::to_string(mPort);
	std::unique_ptr<addrinfo, decltype(freeaddrinfo)*> addressListPtr(nullptr, freeaddrinfo);

	if (mSock == INVALID_SOCKET)
		throw HttpServerException("Could not create socket");

	if (setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(optval)))
		throw HttpServerException(error);

	{
		addrinfo *list = NULL, hint = { 0 };
		hint.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
		hint.ai_family = AF_INET; //IPv4
		hint.ai_socktype = SOCK_STREAM;
		hint.ai_protocol = IPPROTO_TCP;
		if (getaddrinfo(nullptr, strPort.c_str(), &hint, &list))
			throw HttpServerException(error);
		addressListPtr.reset(list);
	}

	auto len = addressListPtr->ai_addrlen;

	if (bind(mSock, addressListPtr->ai_addr, len) == SOCKET_ERROR)
		throw HttpServerException(error);
}
catch (const std::runtime_error e) {
	throw HttpServerException(e.what());
}

void Http::HttpServer::Impl::start()
{
	mServerThread = std::thread(&Impl::serverProcedure, this);
	std::unique_lock<std::mutex> lck(mServerMutex);

	mStatusChanged.wait(lck, [this]() -> bool { return mStatus.load() != ServerStatus::UNINITIALIZED; });

	if (mStatus.load() != ServerStatus::RUNNING) {
		throw HttpServerException("Could not start server");
	}
}

void Http::HttpServer::Impl::setLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mLogger = (callback ? callback : placeholderLogger);
}

void Http::HttpServer::Impl::setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback)
{
	mHandlers[path] = callback;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::HttpServer::HttpServer(std::uint16_t mPort)
	:mThis(new Impl(mPort))
{}

Http::HttpServer::~HttpServer() noexcept = default;

Http::HttpServer::HttpServer(HttpServer &&other) noexcept = default;

Http::HttpServer& Http::HttpServer::operator=(HttpServer &&other) noexcept = default;

void Http::HttpServer::start()
{
	mThis->start();
}

void Http::HttpServer::setLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mThis->setLogger(callback);
}

void Http::HttpServer::setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback)
{
	mThis->setResourceCallback(path, callback);
}

Http::HttpServerException::HttpServerException(const std::string &msg)
	:runtime_error(msg)
	/*#ifdef _WIN32
	,errorCode(WSAGetLastError())
	#elif defined(__linux__)
	,errorCode(errno)
	#endif*/
{}