#ifndef __HTTPRESPONSE__
#define __HTTPRESPONSE__
#include "ExportMacros.h"
#include <vector>
#include <string_view>
#include <optional>
#include <memory>

class Socket;

namespace Http
{
	class EXPORT Response
	{
		class Impl;
		Impl *mThis;
	public:
		enum class HeaderField;
		Response(std::shared_ptr<Socket>);
		~Response() noexcept;
		Response(Response&&) noexcept;
		Response& operator=(Response&&) noexcept;

		void setBody(const std::vector<std::uint8_t> &body);
		void setBody(std::string_view body);
		void setStatusCode(std::uint16_t code);
		void setField(HeaderField field, std::string_view value);
		void setField(std::string_view field, std::string_view value);
		std::optional<std::string_view> getField(HeaderField field);
		std::optional<std::string_view> getField(std::string_view field);
		void sendHeaders();
		void sendBytes(const std::vector<std::uint8_t> &bytes);
		void send();
	};

	using ResponseException = std::runtime_error;

	enum class Response::HeaderField //https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_response_fields
	{
		AccessControlAllowOrigin,
		AccessControlAllowCredentials,
		AccessControlExposeHeaders,
		AccessControlMaxAge,
		AccessControlAllowMethods,
		AccessControlAllowHeaders,
		AcceptPatch,
		AcceptRanges,
		Age,
		Allow,
		AltSvc,
		CacheControl,
		Connection,
		ContentDisposition,
		ContentEncoding,
		ContentLanguage,
		ContentLength,
		ContentLocation,
		ContentMD5,
		ContentRange,
		ContentType, //https://developer.mozilla.org/es/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Lista_completa_de_tipos_MIME
		Date,
		DeltaBase,
		ETag,
		Expires,
		IM,
		LastModified,
		Link,
		Location,
		P3P,
		Pragma,
		ProxyAuthenticate,
		PublicKeyPins,
		RetryAfter,
		Server,
		SetCookie,
		StrictTransportSecurity,
		Trailer,
		TransferEncoding,
		Tk,
		Upgrade,
		Vary,
		Via,
		Warning,
		WWWAuthenticate,
		XFrameOptions
	};
}

#endif