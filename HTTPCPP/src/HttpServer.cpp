#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <string>
#include <algorithm>
#include "Common.h"
#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "RequestScheduler.h"

namespace
{
	void placeholderLogger(const std::string_view&)
	{}
}

class Http::Server::Impl
{
	enum class ServerStatus : std::uint8_t { UNINITIALIZED = 1, RUNNING, STOPPED };
	WinsockLoader mLoader;
	std::map<std::string, std::function<HandlerCallback>> mHandlers;
	std::mutex mServerMutex; //to prevent data races between the thread that created the object and the thread that serves requests
	std::thread mServerThread;
	std::condition_variable mStatusChanged;
	
	std::function<LoggerCallback> mEndpointLogger = placeholderLogger;
	std::function<LoggerCallback> mErrorLogger = placeholderLogger;
	DescriptorType mSock;
	const int mQueueLength;
	std::uint16_t mPort;
	std::atomic<ServerStatus> mStatus; //1 byte

	void serverProcedure();
	void handleRequest(DescriptorType) const;
public:
	Impl(std::uint16_t port, int);
	~Impl();

	void start();
	void setEndpointLogger(const std::function<LoggerCallback> &callback) noexcept;
	void setErrorLogger(const std::function<LoggerCallback> &callback) noexcept;
	void setResourceCallback(const std::string_view &path, const std::function<HandlerCallback> &callback);
};

void Http::Server::Impl::serverProcedure()
{
	if (listen(mSock, mQueueLength) != SOCKET_ERROR)
		mStatus.store(ServerStatus::RUNNING);
	else
		mStatus.store(ServerStatus::STOPPED);

	mStatusChanged.notify_all();

	std::unique_ptr<IRequestScheduler> scheduler(new RequestScheduler(mSock, std::thread::hardware_concurrency(), 5000));

	while (mStatus.load() == ServerStatus::RUNNING)
		scheduler->handleRequest(std::bind(&Impl::handleRequest, this, std::placeholders::_1));
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
	try
	{
		Request request(SocketWrapper{ clientSocket });
		decltype(mHandlers)::const_iterator bestMatch = mHandlers.cend();

		for (decltype(mHandlers)::const_iterator handlerSlot = mHandlers.cbegin(); handlerSlot != mHandlers.cend(); ++handlerSlot)
		{
			std::string_view requestResource = request.getResourcePath();
			std::string::size_type lastSlash = requestResource.rfind('/');

			if (lastSlash != std::string::npos)
			{
				std::string::size_type matchPos = requestResource.find(handlerSlot->first);
				if (matchPos == 0
					&& !handlerSlot->first.compare(0, lastSlash + 1, requestResource, 0, lastSlash + 1)
					)
				{ //if it matches at the beginning, and to the last slash
					if (bestMatch == mHandlers.cend() || handlerSlot->first.size() > bestMatch->first.size())
						bestMatch = handlerSlot;
				}
			}
		}

		if (bestMatch != mHandlers.cend())
		{
			std::string logMessage("Served request at endpoint \"");
			try {
				Response response(SocketWrapper{ clientSocket });
				bestMatch->second(request, response);

				auto requestConnectionHeader = request.getField(Request::HeaderField::Connection), responseConnectionHeader = response.getField(Response::HeaderField::Connection);

				if (requestConnectionHeader && responseConnectionHeader)
				{
					std::string requestConnectionHeaderCopy = requestConnectionHeader.value().data();
					std::transform(requestConnectionHeaderCopy.begin(), requestConnectionHeaderCopy.end(), requestConnectionHeaderCopy.begin(), tolower); //Edge sends Keep-alive, instead of keep-alive

					if (!(requestConnectionHeaderCopy == "keep-alive" && responseConnectionHeader.value() == requestConnectionHeaderCopy))
						CloseSocket(clientSocket);
				}

				logMessage += bestMatch->first;
				logMessage.push_back('\"');
				mEndpointLogger(logMessage);
			}
			catch (const std::exception &e)
			{
				Response serverErrorResponse(SocketWrapper{ clientSocket });

				logMessage = "Exception thrown at endpoint ";
				logMessage.append(bestMatch->first);
				logMessage.append(": ");
				logMessage.append(e.what());
				mEndpointLogger(logMessage);

				serverErrorResponse.setStatusCode(500);
				serverErrorResponse.setField(Response::HeaderField::CacheControl, "no-store");
				serverErrorResponse.setField(Response::HeaderField::Connection, "close");
				serverErrorResponse.send();
				CloseSocket(clientSocket);
			}
		}
	}
	catch (const RequestException &e) {
		mEndpointLogger(e.what());
		CloseSocket(clientSocket);
	}
}

Http::Server::Impl::Impl(std::uint16_t port, int connectionQueueLength)
	:mSock(socket(AF_INET, SOCK_STREAM, 0))
	,mPort(port)
	,mStatus(ServerStatus::UNINITIALIZED)
	,mEndpointLogger(placeholderLogger)
	,mQueueLength(connectionQueueLength)
{
	char optval[8] = {};
	const char *error = "Could not create server";
	std::string strPort = std::to_string(port);
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

void Http::Server::Impl::start()
{
	mServerThread = std::thread(&Impl::serverProcedure, this);
	std::unique_lock<std::mutex> lck(mServerMutex);

	mStatusChanged.wait(lck, [this]() -> bool { return mStatus.load() != ServerStatus::UNINITIALIZED; });

	if (mStatus.load() != ServerStatus::RUNNING) {
		throw ServerException("Could not start server");
	}
}

void Http::Server::Impl::setEndpointLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mEndpointLogger = (callback ? callback : placeholderLogger);
}

void Http::Server::Impl::setErrorLogger(const std::function<LoggerCallback>& callback) noexcept
{
	mErrorLogger = (callback ? callback : placeholderLogger);
}

void Http::Server::Impl::setResourceCallback(const std::string_view &path, const std::function<HandlerCallback> &callback)
{
	mHandlers[path.data()] = callback;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Server::Server(std::uint16_t mPort, int connectionQueueLength)
	:mThis(new Impl(mPort, connectionQueueLength))
{}

Http::Server::~Server() noexcept
{
	delete mThis;
}

Http::Server::Server(Server &&other) noexcept
	:mThis(other.mThis)
{
	other.mThis = nullptr;
}

Http::Server& Http::Server::operator=(Server &&other) noexcept
{
	delete mThis;
	mThis = other.mThis;
	other.mThis = nullptr;

	return *this;
}

void Http::Server::start()
{
	mThis->start();
}

void Http::Server::setEndpointLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mThis->setEndpointLogger(callback);
}

void Http::Server::setErrorLogger(const std::function<LoggerCallback>& callback) noexcept
{
	mThis->setErrorLogger(callback);
}

void Http::Server::setResourceCallback(const std::string_view &path, const std::function<HandlerCallback> &callback)
{
	mThis->setResourceCallback(path, callback);
}