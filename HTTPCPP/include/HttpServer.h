#ifndef __HTTPSERVERCPP__
#define __HTTPSERVERCPP__
#include "ExportMacros.h"
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <string_view>

namespace Http
{
	class Request;
	class Response;

	class EXPORT Server
	{
		class Impl;
		Impl *mThis;
	public:
		using HandlerCallback = void(Request&, Response&);
		using LoggerCallback = void(std::string_view);

		Server(std::uint16_t port = 80, std::uint16_t portSecure = 443, int connectionQueueLength = 6);
		~Server() noexcept;
		Server(Server&&) noexcept;
		Server& operator=(Server&&) noexcept;

		void start();
		//pass nullptr to remove current logger
		void setEndpointLogger(const std::function<LoggerCallback> &callback) noexcept;
		void setErrorLogger(const std::function<LoggerCallback> &callback) noexcept;
		void setResourceCallback(const std::string_view &path, const std::function<HandlerCallback> &callback);
	};

	//using ServerException = std::runtime_error;
}

#endif