#include <string>
#include <type_traits>
#include <sstream>
#include <map>
#include <vector>
#include <chrono>
#include "HttpResponse.h"
#include "Common.h"

#ifdef __linux__
#include <string.h>
#endif

class Http::Response::Impl
{
	WinsockLoader mLoader;
	std::map<HeaderField, std::string> mFields;
	std::string mVersion;
	std::vector<uint8_t> mBody;
	DescriptorType mSock;
	std::uint16_t mStatusCode;

	static const char* getFieldText(HeaderField field);
public:
	Impl(DescriptorType);

	void setBody(const decltype(mBody)&);
	void setBody(const std::string_view&);
	void setStatusCode(std::uint16_t);
	void setField(HeaderField field, const std::string_view &value);
	std::string_view getField(HeaderField);
	void setTimeout(unsigned);
	void send();
};

const char* Http::Response::Impl::getFieldText(HeaderField field)
{
	switch (field)
	{
		case HeaderField::AccessControlAllowOrigin:
			return "Access-Control-Allow-Origin";
		case HeaderField::AccessControlAllowCredentials:
			return "Access-Control-Allow-Credentials";
		case HeaderField::AccessControlExposeHeaders:
			return "Access-Control-Expose-Headers";
		case HeaderField::AccessControlMaxAge:
			return "Access-Control-Max-Age";
		case HeaderField::AccessControlAllowMethods:
			return "Access-Control-Allow-Methods";
		case HeaderField::AccessControlAllowHeaders:
			return "Access-Control-Allow-Headers";
		case HeaderField::AcceptPatch:
			return "Accept-Patch";
		case HeaderField::AcceptRanges:
			return "Accept-Ranges";
		case HeaderField::Age:
			return "Age";
		case HeaderField::Allow:
			return "Allow";
		case HeaderField::AltSvc:
			return "Alt-Svc";
		case HeaderField::CacheControl:
			return "Cache-Control";
		case HeaderField::Connection:
			return "Connection";
		case HeaderField::ContentDisposition:
			return "Content-Disposition";
		case HeaderField::ContentEncoding:
			return "Content-Encoding";
		case HeaderField::ContentLanguage:
			return "Content-Language";
		case HeaderField::ContentLength:
			return "Content-Length";
		case HeaderField::ContentLocation:
			return "Content-Location";
		case HeaderField::ContentMD5:
			return "Content-MD5";
		case HeaderField::ContentRange:
			return "Content-Range";
		case HeaderField::ContentType:
			return "Content-Type";
		case HeaderField::Date:
			return "Date";
		case HeaderField::DeltaBase:
			return "Delta-Base";
		case HeaderField::ETag:
			return "ETag";
		case HeaderField::Expires:
			return "Expires";
		case HeaderField::IM:
			return "IM";
		case HeaderField::LastModified:
			return "Last-Modified";
		case HeaderField::Link:
			return "Link";
		case HeaderField::Location:
			return "Location";
		case HeaderField::P3P:
			return "P3P";
		case HeaderField::Pragma:
			return "Pragma";
		case HeaderField::ProxyAuthenticate:
			return "Proxy-Authenticate";
		case HeaderField::PublicKeyPins:
			return "Public-Key-Pins";
		case HeaderField::RetryAfter:
			return "Retry-After";
		case HeaderField::Server:
			return "Server";
		case HeaderField::SetCookie:
			return "Set-Cookie";
		case HeaderField::StrictTransportSecurity:
			return "Strict-Transport-Security";
		case HeaderField::Trailer:
			return "Trailer";
		case HeaderField::TransferEncoding:
			return "Transfer-Encoding";
		case HeaderField::Tk:
			return "Tk";
		case HeaderField::Upgrade:
			return "Upgrade";
		case HeaderField::Vary:
			return "Vary";
		case HeaderField::Via:
			return "Via";
		case HeaderField::Warning:
			return "Warning";
		case HeaderField::WWWAuthenticate:
			return "WWW-Authenticate";
		case HeaderField::XFrameOptions:
			return "X-Frame-Options";
		default:
			return nullptr;
	}
}

Http::Response::Impl::Impl(DescriptorType sock)
	:mSock(sock)
	,mStatusCode(0)
	,mVersion("1.1")
{
	setTimeout(5);
}

void Http::Response::Impl::setBody(const decltype(mBody) &newBody)
{
	mBody = newBody;
}

void Http::Response::Impl::setBody(const std::string_view &body)
{
	mBody = decltype(mBody)(body.begin(), body.end());
}

void Http::Response::Impl::setStatusCode(std::uint16_t code)
{
	mStatusCode = code;
}

void Http::Response::Impl::setField(HeaderField field, const std::string_view &value)
{
	mFields[field] = value;
}

std::string_view Http::Response::Impl::getField(Http::Response::HeaderField field)
{
	try {
		return mFields.at(field);
	}
	catch (const std::out_of_range&) {
		return "";
	}
}

void Http::Response::Impl::setTimeout(unsigned milliseconds)
{
	#ifdef _WIN32
	DWORD time = milliseconds;
	#elif defined(__linux__)
	struct timeval time = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
	#endif
	setsockopt(mSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&time, sizeof(time));
}

void Http::Response::Impl::send()
{
	if (!mStatusCode)
		throw ResponseException("No status code set");

	const char *fieldEnd("\r\n");

	decltype(MySend(DescriptorType(), nullptr, 0, 0)) bytesSent = 0;

	std::string response = "HTTP/" + mVersion + ' ' + std::to_string(mStatusCode) + fieldEnd;

	for (const auto &fieldValue : mFields) {
		response += getFieldText(fieldValue.first);
		response += ':';
		response += ' ';
		response += fieldValue.second;
		response += fieldEnd;
	}

	response += fieldEnd;

	response.insert(response.end(), mBody.begin(), mBody.end());

	while (bytesSent < (decltype(bytesSent))response.size())
	{
		decltype(bytesSent) auxBytesSent = MySend(mSock, response.data() + bytesSent, response.size() - bytesSent, 0);

		if (auxBytesSent > 0)
			bytesSent += auxBytesSent;
		else /*if (auxBytesSent == SOCKET_ERROR)*/ {
			#ifdef _WIN32
			LPSTR message;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
			std::string strMsg(message);
			LocalFree(message);
			throw ResponseException(strMsg);
			#elif defined(__linux__)
			throw ResponseException(strerror(errno));
			#endif
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Response::Response(const SocketWrapper &wrapper)
	:mThis(new Impl(wrapper.mSock))
{}

Http::Response::~Response() noexcept
{
	delete mThis;
}

Http::Response::Response(Response &&other) noexcept
	:mThis(other.mThis)
{
	other.mThis = nullptr;
}

Http::Response& Http::Response::operator=(Response &&other) noexcept
{
	delete mThis;
	mThis = other.mThis;
	other.mThis = nullptr;

	return *this;
}

void Http::Response::setBody(const std::vector<std::uint8_t> & mBody)
{
	mThis->setBody(mBody);
}

void Http::Response::setBody(const std::string_view &mBody)
{
	mThis->setBody(mBody);
}

void Http::Response::setStatusCode(std::uint16_t code)
{
	mThis->setStatusCode(code);
}

void Http::Response::setField(HeaderField field, const std::string_view &value)
{
	mThis->setField(field, value);
}

std::string_view Http::Response::getField(HeaderField field)
{
	return mThis->getField(field);
}

void Http::Response::setTimeout(int milliseconds)
{
	mThis->setTimeout(milliseconds);
}

void Http::Response::send()
{
	mThis->send();
}