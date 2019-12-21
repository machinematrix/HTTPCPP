#ifndef __HTTPREQUEST__
#define __HTTPREQUEST__
#include "ExportMacros.h"
#include <memory>
#include <string>
#include <cstdint>
#include <vector>

namespace Http
{
	struct SocketWrapper;
	class EXPORT Request
	{
		friend class EXPORT Response;
		class Impl;
		std::unique_ptr<Impl> mThis;
	public:
		enum class HeaderField : std::size_t;
		enum class Status { OK, MALFORMED, EMPTY };

		Request(const SocketWrapper&);
		~Request() noexcept;
		Request(Request&&) noexcept;
		Request& operator=(Request&&) noexcept;

		std::string getMethod();
		std::string getResourcePath();
		std::string getVersion();
		std::string getField(HeaderField field);
		std::string getRequestStringValue(const std::string &key);
		std::vector<std::string> getRequestStringKeys();
		const std::vector<std::uint8_t>& getBody();
		Status getStatus();
		void toggleKeepAlive(bool toggle);
	};

	using RequestException = std::runtime_error;

	enum class Request::HeaderField : std::size_t //https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_request_fields
	{
		AIM,
		Accept,
		AcceptCharset,
		AcceptDatetime,
		AcceptEncoding,
		AcceptLanguage,
		AccessControlRequestMethod,
		AccessControlRequestHeaders,
		Authorization,
		CacheControl,
		Connection,
		ContentLength,
		ContentMD5,
		ContentType,
		Cookie,
		Date,
		Expect,
		Forwarded,
		From,
		Host,
		HTTP2Settings,
		IfMatch,
		IfModifiedSince,
		IfNoneMatch,
		IfRange,
		IfUnmodifiedSince,
		MaxForwards,
		Origin,
		Pragma,
		ProxyAuthorization,
		Range,
		Referer,
		TE,
		Trailer,
		TransferEncoding,
		UserAgent,
		Upgrade,
		Via,
		Warning, //last field
		Invalid
	};
}

#endif