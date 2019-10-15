#ifndef __HTTPSERVERCPP__
#define __HTTPSERVERCPP__
#include <cstdint>
#include <memory>
#include <string_view>
#include <stdexcept>
#include <functional>

namespace Http
{
	class HttpRequest;

	class HttpServer
	{
		class Impl;
		std::unique_ptr<Impl> mThis;
	public:
		using HandlerCallback = void(HttpRequest&);
		using LoggerCallback = void(const std::string_view&);
		enum class ServerStatus : std::uint8_t { UNINITIALIZED = 1, RUNNING = 2, STOPPED };

		HttpServer(std::uint16_t mPort);
		~HttpServer() noexcept;
		HttpServer(HttpServer&&) noexcept;
		HttpServer& operator=(HttpServer&&) noexcept;

		void start();
		//pass nullptr to remove current logger
		void setLogger(const std::function<LoggerCallback> &callback) noexcept;
		void setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback);
	};

	//move to its own file later
	class HttpServerException : public std::runtime_error
	{
		//int errorCode;
	public:
		HttpServerException(const std::string &msg);
	};
}

#endif