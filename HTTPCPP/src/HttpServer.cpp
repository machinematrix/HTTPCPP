#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ThreadPool.h"
#include "Socket.h"

#ifdef _WIN32
#elif defined(__linux__)
#include <sys/socket.h>
#endif

namespace
{
	void placeholderLogger(const std::string_view&)
	{}

	struct SocketInfo
	{
		std::atomic<bool> mIsBeingServed;
		std::chrono::steady_clock::time_point mLastServedTimePoint;

		SocketInfo(const std::chrono::steady_clock::time_point &creation);
		SocketInfo(const SocketInfo&);
		SocketInfo& operator=(const SocketInfo&);
	};

	SocketInfo::SocketInfo(const std::chrono::steady_clock::time_point &creation)
		:mLastServedTimePoint(creation),
		mIsBeingServed(false)
	{}

	SocketInfo::SocketInfo(const SocketInfo &other)
		: mLastServedTimePoint(other.mLastServedTimePoint),
		mIsBeingServed(other.mIsBeingServed.load())
	{}

	SocketInfo& SocketInfo::operator=(const SocketInfo &other)
	{
		mLastServedTimePoint = other.mLastServedTimePoint;
		mIsBeingServed.store(other.mIsBeingServed.load());

		return *this;
	}
}

class Http::Server::Impl
{
public:
	enum class ServerStatus : std::uint8_t { UNINITIALIZED = 1, RUNNING, STOPPED };
	std::map<std::string, std::function<HandlerCallback>> mHandlers;
	std::mutex mServerMutex; //to prevent data races between the thread that created the object and the thread that serves requests
	std::thread mServerThread;
	std::condition_variable mStatusChanged;
	
	std::function<LoggerCallback> mEndpointLogger = placeholderLogger;
	std::function<LoggerCallback> mErrorLogger = placeholderLogger;
	std::shared_ptr<Socket> mSock, mSockSecure;
	const int mQueueLength;
	std::uint16_t mPort, mPortSecure;
	std::atomic<ServerStatus> mStatus; //1 byte

	void serverProcedure();
	void serve(std::shared_ptr<Socket>, decltype(PollFileDescriptor::revents), ThreadPool&, SocketPoller&, std::unordered_map<std::shared_ptr<Socket>, SocketInfo>&, std::chrono::milliseconds);
	void dispatch(std::unordered_map<std::shared_ptr<Socket>, SocketInfo>::iterator);
	void handleRequest(std::shared_ptr<Socket>) const;

	Impl(std::uint16_t, std::uint16_t, int, std::string_view, std::string_view);
	~Impl();
};

void Http::Server::Impl::serverProcedure()
{
	using namespace std::placeholders;
	try {
		if (mSock)
			mSock->listen(mQueueLength);
		if (mSockSecure)
		mSockSecure->listen(mQueueLength);
		mStatus.store(ServerStatus::RUNNING);
	}
	catch (const std::runtime_error &e) {
		mErrorLogger(e.what());
		mStatus.store(ServerStatus::STOPPED);
	}

	mStatusChanged.notify_all();

	std::unordered_map<std::shared_ptr<Socket>, SocketInfo> mSocketInfo;
	SocketPoller poller;
	ThreadPool pool(static_cast<size_t>(std::thread::hardware_concurrency()) * 2ull);
	std::chrono::milliseconds socketTTL(5000);

	if (mSock)
		poller.addSocket(mSock, POLLIN);

	if (mSockSecure)
	poller.addSocket(mSockSecure, POLLIN);

	while (mStatus.load() == ServerStatus::RUNNING)
	{
		//scheduler->handleRequest(std::bind(&Impl::handleRequest, this, std::placeholders::_1));
		poller.poll(1000, std::bind(&Impl::serve, this, _1, _2, std::ref(pool), std::ref(poller), std::ref(mSocketInfo), socketTTL));
	}
}
 
void Http::Server::Impl::serve(std::shared_ptr<Socket> socket, decltype(PollFileDescriptor::revents) revents, ThreadPool &mPool, SocketPoller &poller, std::unordered_map<std::shared_ptr<Socket>, SocketInfo> &mSocketInfo, std::chrono::milliseconds mSocketTimeToLive)
{
	using std::chrono::steady_clock;

	if (revents & POLLIN)
	{
		if (socket == mSock || socket == mSockSecure) //if it's the server socket
		{
			std::shared_ptr<Socket> clientSocket(socket->accept());

			#ifndef NDEBUG
			mEndpointLogger(std::string(__func__) + ' ' + "accept() returned new socket ");
			#endif
			poller.addSocket(clientSocket, POLLIN);

			if (!mSocketInfo.emplace(clientSocket, steady_clock::now()).second)
			{
				//#ifndef NDEBUG
				//std::cout << __func__ << ' ' << "SOCKET RETURNED FROM accept() (" << clientSocket << ") ALREADY EXISTED ON THE MAP" << std::endl;
				//#endif
			}
		}
		else
		{
			auto it = mSocketInfo.find(socket);

			if (!it->second.mIsBeingServed.load())
			{
				//#ifndef NDEBUG
				//cout << __func__ << ' ' << "Serving request with socket: " << socket << endl;
				//#endif

				it->second.mIsBeingServed.store(true);
				mPool.addTask(std::bind(&Impl::dispatch, this, it));
			}
		}
	}
	else if (socket != mSock &&
			 socket != mSockSecure &&
			 revents & POLLNVAL &&
			 !mSocketInfo.find(socket)->second.mIsBeingServed.load())
	{ //if I closed the socket after handling the request, stop monitoring it
		//#ifndef NDEBUG
		//cout << __func__ << ' ' << "Socket " << socket << " was closed by me" << endl;
		//#endif

		mSocketInfo.erase(mSocketInfo.find(socket));
		poller.removeSocket(*socket);
		//mSockets.erase(mSockets.begin() + i);
	}
	else if (socket != mSock &&
			 socket != mSockSecure &&
			 !mSocketInfo.find(socket)->second.mIsBeingServed.load() &&
			 (revents & POLLHUP || steady_clock::now() - mSocketInfo.find(socket)->second.mLastServedTimePoint > mSocketTimeToLive))
	{ //if the other side disconnected or if the sockets TTL has expired, close the socket.
		//#ifndef NDEBUG
		//cout << __func__ << ' ' << "Socket " << socket << " expired or got hung up, closing it..." << endl;
		//#endif

		mSocketInfo.erase(mSocketInfo.find(socket));
		poller.removeSocket(*socket);
	}
}

void Http::Server::Impl::dispatch(std::unordered_map<std::shared_ptr<Socket>, SocketInfo>::iterator it)
{
	handleRequest(it->first);
	it->second.mLastServedTimePoint = std::chrono::steady_clock::now();
	it->second.mIsBeingServed.store(false);
}

Http::Server::Impl::~Impl()
{
	mStatus.store(ServerStatus::STOPPED);
	if (mSock)
		mSock->close();
	if (mSockSecure)
		mSockSecure->close();
	if (mServerThread.joinable())
		mServerThread.join();
}

void Http::Server::Impl::handleRequest(std::shared_ptr<Socket> clientSocket) const
{
	try
	{
		Request request(clientSocket);
		decltype(mHandlers)::const_iterator bestMatch = mHandlers.cend();

		for (decltype(mHandlers)::const_iterator handlerSlot = mHandlers.cbegin(); handlerSlot != mHandlers.cend(); ++handlerSlot)
		{
			std::string_view requestResource = request.getResourcePath();
			std::string::size_type lastSlash = requestResource.rfind('/');

			if (lastSlash != std::string::npos)
			{
				std::string::size_type matchPos = requestResource.find(handlerSlot->first);
				if (!matchPos && !handlerSlot->first.compare(0, lastSlash + 1, requestResource, 0, lastSlash + 1))
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
				Response response(clientSocket);
				bestMatch->second(request, response);

				auto requestConnectionHeader = request.getField(Request::HeaderField::Connection), responseConnectionHeader = response.getField(Response::HeaderField::Connection);

				if (requestConnectionHeader && responseConnectionHeader)
				{
					std::string requestConnectionHeaderCopy = requestConnectionHeader.value().data();
					std::transform(requestConnectionHeaderCopy.begin(), requestConnectionHeaderCopy.end(), requestConnectionHeaderCopy.begin(), tolower); //Edge sends Keep-alive, instead of keep-alive

					if (!(requestConnectionHeaderCopy == "keep-alive" && responseConnectionHeader.value() == requestConnectionHeaderCopy))
						clientSocket->close();
				}

				logMessage += bestMatch->first;
				logMessage.push_back('\"');
				mEndpointLogger(logMessage);
			}
			catch (const std::exception &e)
			{
				Response serverErrorResponse(clientSocket);

				logMessage = "Exception thrown at endpoint ";
				logMessage.append(bestMatch->first);
				logMessage.append(": ");
				logMessage.append(e.what());
				mEndpointLogger(logMessage);

				serverErrorResponse.setStatusCode(500);
				serverErrorResponse.setField(Response::HeaderField::CacheControl, "no-store");
				serverErrorResponse.setField(Response::HeaderField::Connection, "close");
				serverErrorResponse.send();
				clientSocket->close();
			}
		}
	}
	catch (const RequestException &e) {
		mEndpointLogger(e.what());
		clientSocket->close();
	}
}

Http::Server::Impl::Impl(std::uint16_t port, std::uint16_t portSecure, int connectionQueueLength, std::string_view certificateStore, std::string_view certificateName)
	:mSock(port ? new Socket(AF_INET, SOCK_STREAM, 0) : nullptr)
	,mSockSecure(portSecure ? new TLSSocket(AF_INET, SOCK_STREAM, 0, certificateStore, certificateName) : nullptr)
	,mPort(port)
	,mPortSecure(portSecure)
	,mStatus(ServerStatus::UNINITIALIZED)
	,mEndpointLogger(placeholderLogger)
	,mQueueLength(connectionQueueLength)
{
	if (mSock)
		mSock->bind("0.0.0.0", mPort, true);

	if (mSockSecure)
		mSockSecure->bind("0.0.0.0", mPortSecure, true);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Server::Server(std::uint16_t port, std::uint16_t portSecure, int connectionQueueLength, std::string_view certificateStore, std::string_view certificateName)
	:mThis(new Impl(port, portSecure, connectionQueueLength, certificateStore, certificateName))
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
	mThis->mServerThread = std::thread(&Impl::serverProcedure, mThis);
	std::unique_lock<std::mutex> lck(mThis->mServerMutex);

	mThis->mStatusChanged.wait(lck, [this]() -> bool { return mThis->mStatus.load() != Impl::ServerStatus::UNINITIALIZED; });

	if (mThis->mStatus.load() != Impl::ServerStatus::RUNNING)
		throw std::runtime_error("Could not start server");
}

void Http::Server::setEndpointLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mThis->mEndpointLogger = callback ? callback : placeholderLogger;
}

void Http::Server::setErrorLogger(const std::function<LoggerCallback> &callback) noexcept
{
	mThis->mErrorLogger = callback ? callback : placeholderLogger;
}

void Http::Server::setResourceCallback(const std::string_view &path, const std::function<HandlerCallback> &callback)
{
	mThis->mHandlers[path.data()] = callback;
}