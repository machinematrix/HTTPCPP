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
	std::shared_ptr<Socket> mSocket, mSocketSecure;
	const int mQueueLength;
	std::uint16_t mPort, mPortSecure;
	std::atomic<ServerStatus> mStatus; //1 byte

	void serverProcedure();
	void handleRequest(std::shared_ptr<Socket>) const;

	Impl(std::uint16_t, std::uint16_t, int, std::string_view, std::string_view);
	~Impl();
};

void Http::Server::Impl::serverProcedure()
{
	using namespace std::placeholders;
	try
	{
		if (mSocket)
			mSocket->listen(mQueueLength);
		if (mSocketSecure)
			mSocketSecure->listen(mQueueLength);
		mStatus.store(ServerStatus::RUNNING);
	}
	catch (const std::runtime_error &e)
	{
		mErrorLogger(e.what());
		mStatus.store(ServerStatus::STOPPED);
	}

	//ThreadPool pool(static_cast<size_t>(std::thread::hardware_concurrency()) * 2ull);
	mStatusChanged.notify_all();
	std::vector<PollFileDescriptor> descriptorList;
	std::vector<std::pair<std::shared_ptr<Socket>, decltype(descriptorList)::size_type>> socketList;

	if (mSocket)
	{
		descriptorList.push_back({ mSocket->get(), POLLIN });
		socketList.emplace_back(mSocket, descriptorList.size() - 1);
	}

	if (mSocketSecure)
	{
		descriptorList.push_back({ mSocketSecure->get(), POLLIN });
		socketList.emplace_back(mSocketSecure, descriptorList.size() - 1);
	}

	while (mStatus.load() == ServerStatus::RUNNING)
	{
		if (auto returnValue = WSAPoll(descriptorList.data(), static_cast<ULONG>(descriptorList.size()), 1000); returnValue != SOCKET_ERROR) //add return value check later!
		{
			for (auto &entry : socketList)
				if (descriptorList[entry.second].revents & POLLIN)
					std::thread(&Impl::handleRequest, this, std::shared_ptr<Socket>(entry.first->accept())).detach();
		}
		else
		{
			mStatus.store(ServerStatus::STOPPED);
			mErrorLogger("poll error code " + std::to_string(returnValue) + ", server stopped");
		}
	}
}

Http::Server::Impl::~Impl()
{
	mStatus.store(ServerStatus::STOPPED);
	if (mSocket)
		mSocket->close();
	if (mSocketSecure)
		mSocketSecure->close();
	if (mServerThread.joinable())
		mServerThread.join();
}

void Http::Server::Impl::handleRequest(std::shared_ptr<Socket> clientSocket) const
{
	DWORD timeout = 5000;
	mEndpointLogger("Connected socket " + std::to_string(clientSocket->get()));
	clientSocket->setSocketOption(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	clientSocket->setSocketOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	try
	{
		while (true)
		{
			try
			{
				char c;
				clientSocket->Socket::receive(&c, 1, MSG_PEEK); //Wait until there's data to read
			}
			catch (const SocketException &e)
			{
				using namespace std::string_literals;
				if (e.getErrorCode() == WSAETIMEDOUT) //Ok, keep-alive timeout expired
					mEndpointLogger("keep-alive expired on socket " + std::to_string(clientSocket->get()));
				else
					mErrorLogger(e.what());
				break;
			}
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
				std::string logMessage("Served request at endpoint \"" + bestMatch->first + '\"');
				try
				{
					Response response(clientSocket);
					bestMatch->second(request, response);

					auto requestConnectionHeader = request.getField(Request::HeaderField::Connection), responseConnectionHeader = response.getField(Response::HeaderField::Connection);

					if (requestConnectionHeader && responseConnectionHeader)
					{
						std::string requestConnectionHeaderCopy = requestConnectionHeader.value().data();
						std::transform(requestConnectionHeaderCopy.begin(), requestConnectionHeaderCopy.end(), requestConnectionHeaderCopy.begin(), tolower); //Edge sends Keep-alive, instead of keep-alive

						if (!(requestConnectionHeaderCopy == "keep-alive" && responseConnectionHeader.value() == requestConnectionHeaderCopy)) //No keep-alive, exit...
						{
							mEndpointLogger(logMessage);
							break;
						}
					}

					mEndpointLogger(logMessage);
				}
				catch (const std::exception &e)
				{
					Response serverErrorResponse(clientSocket);

					logMessage = "Exception thrown at endpoint ";
					logMessage.append(bestMatch->first);
					logMessage.append(": ");
					logMessage.append(e.what());
					mErrorLogger(logMessage);

					serverErrorResponse.setStatusCode(500);
					serverErrorResponse.setField(Response::HeaderField::CacheControl, "no-store");
					serverErrorResponse.setField(Response::HeaderField::Connection, "close");
					serverErrorResponse.send();
					break;
				}
			}
		}
	}
	catch (const RequestException &e)
	{
		mErrorLogger(e.what());
	}
}

Http::Server::Impl::Impl(std::uint16_t port, std::uint16_t portSecure, int connectionQueueLength, std::string_view certificateStore, std::string_view certificateName)
	:mSocket(port ? new Socket(AF_INET, SOCK_STREAM, 0) : nullptr)
	,mSocketSecure(portSecure ? new TLSSocket(AF_INET, SOCK_STREAM, 0, certificateStore, certificateName) : nullptr)
	,mPort(port)
	,mPortSecure(portSecure)
	,mStatus(ServerStatus::UNINITIALIZED)
	,mEndpointLogger(placeholderLogger)
	,mQueueLength(connectionQueueLength)
{
	if (!port && !portSecure)
		throw std::invalid_argument("At least one of the ports must be different than zero");

	if (mSocket)
		mSocket->bind("0.0.0.0", mPort, true);

	if (mSocketSecure)
		mSocketSecure->bind("0.0.0.0", mPortSecure, true);
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