#include <string>
#include <type_traits>
#include <sstream>
#include <map>
#include <array>
#include <limits>
#include <regex>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Common.h"

#ifndef NDEBUG
#include <iostream>
#endif

#undef max

#ifdef __linux__
inline void Sleep(size_t miliseconds)
{
	usleep(miliseconds * 1000);
}
#endif

#ifdef _WIN32
//sets socket to non blocking mode on construction
class NonBlockSocket
{
	DescriptorType mSocket;
	u_long toggle;
public:
	NonBlockSocket(DescriptorType sock)
		:mSocket(sock)
		,toggle(1)
	{
		ioctlsocket(mSocket, FIONBIO, &toggle);
	}

	~NonBlockSocket()
	{
		toggle = 0;
		ioctlsocket(mSocket, FIONBIO, &toggle);
	}

	NonBlockSocket(const NonBlockSocket&&) = delete;
	NonBlockSocket& operator=(const NonBlockSocket&&) = delete;
};
#endif

class Http::Request::Impl
{
	static std::regex requestHeaderFormat, requestLineFormat, queryStringFormat, queryStringParams;
	WinsockLoader mLoader;
	std::string mMethod;
	std::string mResource;
	std::string mVersion;
	std::array<std::string, static_cast<size_t>(HeaderField::Warning) + 1u> mFields;
	std::vector<std::uint8_t> mBody;
	std::map<std::string, std::string> queryStringArguments;
	DescriptorType mSock;
	Status mStatus;

	static const char* getFieldText(HeaderField field);
	HeaderField getFieldId(const std::string &field);
public:
	Impl(const SocketWrapper &mSock);

	std::string getMethod();
	std::string getResourcePath();
	std::string getVersion();
	std::string getField(HeaderField field);
	std::string getRequestStringValue(const std::string&);
	std::vector<std::string> getRequestStringKeys();

	const decltype(mBody)& getBody();
	Status getStatus();
};

//[1]: header field
//[2]: header value
std::regex Http::Request::Impl::requestHeaderFormat("([^:]+):[[:space:]]?(.+)\r\n");

//[1]: method
//[2]: resource
//[3]: version
std::regex Http::Request::Impl::requestLineFormat("([[:upper:]]+) (/[^[:space:]]*) HTTP/([[:digit:]]+\\.[[:digit:]]+)\r\n");

//[1]: first parameter
std::regex Http::Request::Impl::queryStringFormat(".+[^/](\\?[^\\?/[:space:]&=]+)=(?:[^\\?/[:space:]&=]+)(?:&(?:[^\\?/[:space:]&=]+)=(?:[^\\?/[:space:]&=]+))*");

//[1]: parameter
//[2]: value
std::regex Http::Request::Impl::queryStringParams("([^\\?/[:space:]&=]+)=([^\\?/[:space:]&=]+)");

const char* Http::Request::Impl::getFieldText(HeaderField field)
{
	switch (field)
	{
		case HeaderField::AIM:
			return "A-IM";
		case HeaderField::Accept:
			return "Accept";
		case HeaderField::AcceptCharset:
			return "Accept-Charset";
		case HeaderField::AcceptDatetime:
			return "Accept-Datetime";
		case HeaderField::AcceptEncoding:
			return "Accept-Encoding";
		case HeaderField::AcceptLanguage:
			return "Accept-Language";
		case HeaderField::AccessControlRequestMethod:
			return "Access-Control-Request-Method";
		case HeaderField::AccessControlRequestHeaders:
			return "Access-Control-Request-Method";
		case HeaderField::Authorization:
			return "Authorization";
		case HeaderField::CacheControl:
			return "Cache-Control";
		case HeaderField::Connection:
			return "Connection";
		case HeaderField::ContentLength:
			return "Content-Length";
		case HeaderField::ContentMD5:
			return "Content-MD5";
		case HeaderField::ContentType:
			return "Content-Type";
		case HeaderField::Cookie:
			return "Content-Cookie";
		case HeaderField::Date:
			return "Date";
		case HeaderField::Expect:
			return "Expect";
		case HeaderField::Forwarded:
			return "Forwarded";
		case HeaderField::From:
			return "From";
		case HeaderField::Host:
			return "Host";
		case HeaderField::HTTP2Settings:
			return "HTTP2-Settings";
		case HeaderField::IfMatch:
			return "If-Match";
		case HeaderField::IfModifiedSince:
			return "If-Modified-Since";
		case HeaderField::IfNoneMatch:
			return "If-None-Match";
		case HeaderField::IfRange:
			return "If-Range";
		case HeaderField::IfUnmodifiedSince:
			return "If-Unmodified-Since";
		case HeaderField::MaxForwards:
			return "Max-Forwards";
		case HeaderField::Origin:
			return "Origin";
		case HeaderField::Pragma:
			return "Pragma";
		case HeaderField::ProxyAuthorization:
			return "Proxy-Authorization";
		case HeaderField::Range:
			return "Range";
		case HeaderField::Referer:
			return "Referer";
		case HeaderField::TE:
			return "TE";
		case HeaderField::Trailer:
			return "Trailer";
		case HeaderField::TransferEncoding:
			return "Transfer-Encoding";
		case HeaderField::UserAgent:
			return "User-Agent";
		case HeaderField::Upgrade:
			return "Upgrade";
		case HeaderField::Via:
			return "Via";
		case HeaderField::Warning:
			return "Warning";
		default:
			return NULL;
	}
}

Http::Request::HeaderField Http::Request::Impl::getFieldId(const std::string &field)
{
	static const std::map<std::string, HeaderField> fieldMap = {
		{ getFieldText(HeaderField::AIM), HeaderField::AIM },
		{ getFieldText(HeaderField::Accept), HeaderField::Accept },
		{ getFieldText(HeaderField::AcceptCharset), HeaderField::AcceptCharset },
		{ getFieldText(HeaderField::AcceptDatetime), HeaderField::AcceptDatetime },
		{ getFieldText(HeaderField::AcceptEncoding), HeaderField::AcceptEncoding },
		{ getFieldText(HeaderField::AcceptLanguage), HeaderField::AcceptLanguage },
		{ getFieldText(HeaderField::AccessControlRequestMethod), HeaderField::AccessControlRequestMethod },
		{ getFieldText(HeaderField::AccessControlRequestHeaders), HeaderField::AccessControlRequestHeaders },
		{ getFieldText(HeaderField::Authorization), HeaderField::Authorization },
		{ getFieldText(HeaderField::CacheControl), HeaderField::CacheControl },
		{ getFieldText(HeaderField::Connection), HeaderField::Connection },
		{ getFieldText(HeaderField::ContentLength), HeaderField::ContentLength },
		{ getFieldText(HeaderField::ContentMD5), HeaderField::ContentMD5 },
		{ getFieldText(HeaderField::ContentType), HeaderField::ContentType },
		{ getFieldText(HeaderField::Cookie), HeaderField::Cookie },
		{ getFieldText(HeaderField::Date), HeaderField::Date },
		{ getFieldText(HeaderField::Expect), HeaderField::Expect },
		{ getFieldText(HeaderField::Forwarded), HeaderField::Forwarded },
		{ getFieldText(HeaderField::From), HeaderField::From },
		{ getFieldText(HeaderField::Host), HeaderField::Host },
		{ getFieldText(HeaderField::HTTP2Settings), HeaderField::HTTP2Settings },
		{ getFieldText(HeaderField::IfMatch), HeaderField::IfMatch },
		{ getFieldText(HeaderField::IfModifiedSince), HeaderField::IfModifiedSince },
		{ getFieldText(HeaderField::IfNoneMatch), HeaderField::IfNoneMatch },
		{ getFieldText(HeaderField::IfRange), HeaderField::IfRange },
		{ getFieldText(HeaderField::IfUnmodifiedSince), HeaderField::IfUnmodifiedSince },
		{ getFieldText(HeaderField::MaxForwards), HeaderField::MaxForwards },
		{ getFieldText(HeaderField::Origin), HeaderField::Origin },
		{ getFieldText(HeaderField::Pragma), HeaderField::Pragma },
		{ getFieldText(HeaderField::ProxyAuthorization), HeaderField::ProxyAuthorization },
		{ getFieldText(HeaderField::Range), HeaderField::Range },
		{ getFieldText(HeaderField::Referer), HeaderField::Referer },
		{ getFieldText(HeaderField::TE), HeaderField::TE },
		{ getFieldText(HeaderField::Trailer), HeaderField::Trailer },
		{ getFieldText(HeaderField::TransferEncoding), HeaderField::TransferEncoding },
		{ getFieldText(HeaderField::UserAgent), HeaderField::UserAgent },
		{ getFieldText(HeaderField::Upgrade), HeaderField::Upgrade },
		{ getFieldText(HeaderField::Via), HeaderField::Via },
		{ getFieldText(HeaderField::Warning), HeaderField::Warning },
	};
	
	HeaderField result;
	auto it = fieldMap.find(field);
	
	if (it != fieldMap.end())
		result = it->second;
	else
		result = HeaderField::Invalid;

	return result;
}

Http::Request::Impl::Impl(const SocketWrapper &sockWrapper)
	:mSock(sockWrapper.mSock)
	,mStatus(Status::EMPTY)
{
	using std::string;
	using std::array;
	using std::regex_search;

	#ifdef _WIN32
	int flags = 0;
	NonBlockSocket nonBlock(mSock);
	#elif defined(__linux__)
	int flags = MSG_DONTWAIT;
	#endif

	constexpr unsigned initialChances = 200, sleepTime = 5;
	unsigned contentLength, chances = initialChances;
	string requestText;
	array<string::value_type, 256> buffer;
	string::size_type headerEnd = string::npos;
	std::smatch requestLineMatch, queryStringMatch;

	do
	{
		auto bytesRead = MyRecv(mSock, buffer.data(), buffer.size(), flags);

		if (bytesRead != SOCKET_ERROR && bytesRead > 0)
		{
			requestText.append(buffer.data(), bytesRead);
			headerEnd = requestText.rfind("\r\n\r\n");
			chances = initialChances;
		}
		else
		{
			Sleep(sleepTime);
			--chances;
		}
	}
	while (chances && headerEnd == string::npos);

	if (requestText.empty())
		return;

	if (headerEnd == string::npos) {
		mStatus = Status::MALFORMED;
		return;
	}

	if (regex_search(requestText, requestLineMatch, requestLineFormat))
	{
		mMethod = requestLineMatch[1];
		mResource = requestLineMatch[2];
		mVersion = requestLineMatch[3];

		if (regex_match(mResource, queryStringMatch, queryStringFormat))
		{

			for (std::sregex_iterator it(mResource.begin() + queryStringMatch.position(1), mResource.end(), queryStringParams), end; it != end; ++it)
			{
				queryStringArguments[(*it)[1]] = (*it)[2];
			}
			mResource.erase(mResource.begin() + queryStringMatch.position(1), mResource.end());
		}
	}
	else {
		mStatus = Status::MALFORMED;
		return;
	}

	for (std::sregex_iterator i(requestText.begin(), requestText.end(), requestHeaderFormat), end; i != end; ++i)
	{
		HeaderField id = getFieldId((*i)[1]);

		if (id != HeaderField::Invalid)
		{
			mFields[static_cast<size_t>(id)] = (*i)[2];
		}
	}

	try {
		contentLength = std::stoul(mFields.at(static_cast<decltype(mFields)::size_type>(HeaderField::ContentLength)));
	}
	catch (const std::invalid_argument&) {
		contentLength = 0;
	}

	if (contentLength)
	{
		mBody.insert(mBody.begin(), requestText.begin() + headerEnd + 4, requestText.end());
		chances = initialChances;

		while (mBody.size() < contentLength && chances)
		{
			auto bytesRead = MyRecv(mSock, buffer.data(), buffer.size(), flags);

			if (bytesRead != SOCKET_ERROR && bytesRead > 0)
			{
				mBody.insert(mBody.end(), buffer.begin(), buffer.begin() + bytesRead);
				chances = initialChances;
			}
			else
			{
				Sleep(sleepTime);
				--chances;
			}
		}
	}

	mStatus = Status::OK;
}

std::string Http::Request::Impl::getMethod()
{
	return mMethod;
}

std::string Http::Request::Impl::getResourcePath()
{
	return mResource;
}

std::string Http::Request::Impl::getVersion()
{
	return mVersion;
}

std::string Http::Request::Impl::getField(HeaderField field)
{
	return mFields[static_cast<size_t>(field)];
}

std::string Http::Request::Impl::getRequestStringValue(const std::string &key)
{
	try {
		return queryStringArguments.at(key);
	}
	catch (const std::out_of_range&) {
		throw RequestException("Key does not exist");
	}
}

std::vector<std::string> Http::Request::Impl::getRequestStringKeys()
{
	std::vector<std::string> result;
	result.reserve(queryStringArguments.size());

	for (const decltype(queryStringArguments)::value_type &pair : queryStringArguments) {
		result.push_back(pair.first);
	}

	return result;
}

const decltype(Http::Request::Impl::mBody)& Http::Request::Impl::getBody()
{
	return mBody;
}

Http::Request::Status Http::Request::Impl::getStatus()
{
	return mStatus;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Request::Request(const SocketWrapper &sockWrapper)
	:mThis(new Impl(sockWrapper))
{}

Http::Request::~Request() noexcept = default;

Http::Request::Request(Request&&) noexcept = default;

Http::Request& Http::Request::operator=(Request&&) noexcept = default;

std::string Http::Request::getMethod()
{
	return mThis->getMethod();
}

std::string Http::Request::getResourcePath()
{
	return mThis->getResourcePath();
}

std::string Http::Request::getVersion()
{
	return mThis->getVersion();
}

std::string Http::Request::getField(HeaderField field)
{
	return mThis->getField(field);
}

std::string Http::Request::getRequestStringValue(const std::string &key)
{
	return mThis->getRequestStringValue(key);
}

std::vector<std::string> Http::Request::getRequestStringKeys()
{
	return mThis->getRequestStringKeys();
}

const std::vector<std::uint8_t>& Http::Request::getBody()
{
	return mThis->getBody();
}

Http::Request::Status Http::Request::getStatus()
{
	return mThis->getStatus();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//RESPONSE-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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
	void setBody(const std::string&);
	void setStatusCode(std::uint16_t);
	void setField(HeaderField field, const std::string &value);
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
	:mSock(sock),
	mStatusCode(0),
	mVersion("1.1")
{}

void Http::Response::Impl::setBody(const decltype(mBody) &newBody)
{
	mBody = newBody;
}

void Http::Response::Impl::setBody(const std::string &body)
{
	mBody = decltype(mBody)(body.begin(), body.end());
}

void Http::Response::Impl::setStatusCode(std::uint16_t code)
{
	mStatusCode = code;
}

void Http::Response::Impl::setField(HeaderField field, const std::string &value)
{
	mFields[field] = value;
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
		else {
			break;
			throw ResponseException("");
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

Http::Response::~Response() noexcept = default;

Http::Response::Response(Response &&) noexcept = default;

Http::Response& Http::Response::operator=(Response&&) noexcept = default;

void Http::Response::setBody(const std::vector<std::uint8_t>& mBody)
{
	mThis->setBody(mBody);
}

void Http::Response::setBody(const std::string &mBody)
{
	mThis->setBody(mBody);
}

void Http::Response::setStatusCode(std::uint16_t code)
{
	mThis->setStatusCode(code);
}

void Http::Response::setField(HeaderField field, const std::string &value)
{
	mThis->setField(field, value);
}

void Http::Response::send()
{
	mThis->send();
}