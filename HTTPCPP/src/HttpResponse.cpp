#include <string>
#include <type_traits>
#include <sstream>
#include <map>
#include <vector>
#include <chrono>
#include "HttpResponse.h"
#include "Common.h"
#include "Socket.h"

#ifdef __linux__
#include <cstring>
#endif

class Http::Response::Impl
{
public:
	std::map<std::string, std::string, decltype(CaseInsensitiveComparator)*> mFields;
	std::string mVersion;
	std::vector<uint8_t> mBody;
	std::shared_ptr<Socket> mSock;
	std::uint16_t mStatusCode;

	static const char* getFieldText(HeaderField field);
	Impl(const std::shared_ptr<Socket>&);
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

Http::Response::Impl::Impl(const std::shared_ptr<Socket> &sock)
	:mSock(sock)
	,mStatusCode(0)
	,mVersion("1.1")
	,mFields(CaseInsensitiveComparator)
{
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Response::Response(const std::shared_ptr<Socket> &wrapper)
	:mThis(new Impl(wrapper))
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

void Http::Response::setBody(const std::vector<std::uint8_t> &newBody)
{
	mThis->mBody = newBody;
}

void Http::Response::setBody(std::string_view body)
{
	mThis->mBody = decltype(Impl::mBody)(body.begin(), body.end());
}

void Http::Response::setStatusCode(std::uint16_t code)
{
	mThis->mStatusCode = code;
}

void Http::Response::setField(HeaderField field, std::string_view value)
{
	mThis->mFields[mThis->getFieldText(field)] = value;
}

void Http::Response::setField(std::string_view field, std::string_view value)
{
	mThis->mFields[field.data()] = value;
}

std::optional<std::string_view> Http::Response::getField(HeaderField field)
{
	try {
		return mThis->mFields.at(mThis->getFieldText(field));
	}
	catch (const std::out_of_range&) {
		return std::optional<std::string_view>();
	}
}

std::optional<std::string_view> Http::Response::getField(std::string_view field)
{
	try {
		return mThis->mFields.at(field.data());
	}
	catch (const std::out_of_range&) {
		return std::optional<std::string_view>();
	}
}

void Http::Response::sendHeaders()
{
	if (!mThis->mStatusCode)
		throw ResponseException("No status code set");
	constexpr const char *fieldEnd = "\r\n";
	std::string response = "HTTP/" + mThis->mVersion + ' ' + std::to_string(mThis->mStatusCode) + fieldEnd;
	std::int64_t bytesSent = 0;

	for (const auto &fieldValue : mThis->mFields) {
		response += fieldValue.first;
		response += ": ";
		response += fieldValue.second;
		response += fieldEnd;
	}

	response += fieldEnd;

	while (bytesSent < static_cast<decltype(bytesSent)>(response.size()))
	{
		decltype(bytesSent) auxBytesSent = mThis->mSock->send(response.data() + bytesSent, response.size() - bytesSent, 0);

		if (auxBytesSent > 0)
			bytesSent += auxBytesSent;
		else {
			#ifdef _WIN32
			LPSTR message;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
			std::string strMsg(message);
			LocalFree(message);
			throw ResponseException(strMsg);
			#elif defined(__linux__)
			throw ResponseException(std::strerror(errno));
			#endif
		}
	}
}

void Http::Response::sendBytes(const std::vector<std::uint8_t> &bytes)
{
	std::int64_t bytesSent = 0;

	while (bytesSent < static_cast<decltype(bytesSent)>(bytes.size()))
		bytesSent += mThis->mSock->send(const_cast<std::uint8_t*>(bytes.data()) + bytesSent, bytes.size() - bytesSent, 0);
}

void Http::Response::send()
{
	if (!mThis->mStatusCode)
		throw ResponseException("No status code set");

	constexpr const char *fieldEnd = "\r\n";

	std::int64_t bytesSent = 0;

	std::string response = "HTTP/" + mThis->mVersion + ' ' + std::to_string(mThis->mStatusCode) + fieldEnd;

	for (const auto &fieldValue : mThis->mFields) {
		response += fieldValue.first;
		response += ": ";
		response += fieldValue.second;
		response += fieldEnd;
	}

	response += fieldEnd;

	response.insert(response.end(), mThis->mBody.begin(), mThis->mBody.end());

	while (bytesSent < static_cast<decltype(bytesSent)>(response.size()))
		bytesSent += mThis->mSock->send(response.data() + bytesSent, response.size() - bytesSent, 0);
}