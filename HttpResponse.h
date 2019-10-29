#ifndef __HTTPRESPONSE__
#define __HTTPRESPONSE__
#include "ExportMacros.h"
#include <memory>

namespace Http
{
	class EXPORT HttpRequest;
	class EXPORT HttpResponse
	{
		class Impl;
		std::unique_ptr<Impl> mThis;
	public:
		enum class HeaderField;
		HttpResponse(const HttpRequest&);
		~HttpResponse() noexcept;
		HttpResponse(HttpResponse&&) noexcept;
		HttpResponse& operator=(HttpResponse&&) noexcept;

		void setBody(const std::vector<std::int8_t> &mBody);
		void setStatusCode(std::uint16_t code);
		void setField(HeaderField field, const std::string &value);
		void send();
	};

	enum class HttpResponse::HeaderField //https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_response_fields
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