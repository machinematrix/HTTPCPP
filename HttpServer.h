#ifndef __HTTPSERVERCPP__
#define __HTTPSERVERCPP__
#include "ExportMacros.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <functional>
#include <string>

namespace Http
{
	class Request;

	class EXPORT Server
	{
		class Impl;
		std::unique_ptr<Impl> mThis;
	public:
		using HandlerCallback = void(Request&);
		using LoggerCallback = void(const std::string&);
		

		Server(std::uint16_t mPort);
		~Server() noexcept;
		Server(Server&&) noexcept;
		Server& operator=(Server&&) noexcept;

		void start();
		//pass nullptr to remove current logger
		void setLogger(const std::function<LoggerCallback> &callback) noexcept;
		void setResourceCallback(const std::string &path, const std::function<HandlerCallback> &callback);
	};

	using ServerException = std::runtime_error;
}

#endif