#include <unordered_map>
#include <map>
#include <string>
#include <array>
#include <regex>
#include "HttpRequest.h"
#include "Common.h"
#include "Socket.h"

#ifndef NDEBUG
#include <iostream>
#endif

#undef max

#ifdef __linux__
#include <sys/socket.h>
#include <unistd.h>
inline void Sleep(size_t miliseconds)
{
	usleep(miliseconds * 1000);
}
#endif

class Http::Request::Impl
{
	static std::regex requestHeaderFormat, requestLineFormat, queryStringFormat, queryStringParams;
	std::string mMethod;
	std::string mResource;
	std::string mVersion;
	std::map<std::string, std::string, decltype(CaseInsensitiveComparator)*> mFields;
	std::vector<std::uint8_t> mBody;
	std::unordered_map<std::string, std::string> queryStringArguments;
	std::shared_ptr<Socket> mSock;

	static const char* getFieldText(HeaderField field);
	HeaderField getFieldId(const std::string_view &field);
public:
	Impl(const std::shared_ptr<Socket> &mSock);

	std::string_view getMethod();
	std::string_view getResourcePath();
	std::string_view getVersion();
	std::optional<std::string_view> getField(HeaderField field);
	std::optional<std::string_view> getField(std::string_view field);
	std::optional<std::string_view> getRequestStringValue(std::string_view);
	std::vector<std::string_view> getRequestStringKeys();
	const decltype(mBody)& getBody();
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

Http::Request::HeaderField Http::Request::Impl::getFieldId(const std::string_view &field)
{
	static const std::unordered_map<std::string_view, HeaderField> fieldMap = {
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

Http::Request::Impl::Impl(const std::shared_ptr<Socket> &sockWrapper)
	:mSock(sockWrapper),
	mFields(CaseInsensitiveComparator)
{
	using std::string;
	using std::array;
	using std::regex_search;

	#ifdef _WIN32
	int flags = 0;
	DWORD timeout = 10000;
	//mSock->toggleNonBlockingMode(false);
	//NonBlockingSocket nonBlocking(*mSock);
	mSock->setSocketOption(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	mSock->setSocketOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	#elif defined(__linux__)
	int flags = MSG_DONTWAIT;
	#endif

	unsigned contentLength;
	string requestText;
	array<string::value_type, 1024> buffer;
	string::size_type headerEnd = string::npos;
	std::smatch requestLineMatch, queryStringMatch;
	TLSSocket *tlsSocket = dynamic_cast<TLSSocket*>(mSock.get());
	
	if (tlsSocket)
	{
		do
		{
			auto aux = tlsSocket->receiveTLSMessage(flags);

			if (aux.empty())
			{
				if (headerEnd == std::string::npos)
					throw RequestException("Invalid request");
			}
			else
			{
				requestText += aux;
				headerEnd = requestText.find("\r\n\r\n");
			}
		} while (headerEnd == string::npos);
	}
	else
	{		
		do
		{
			auto bytesRead = mSock->receive(buffer.data(), buffer.size(), flags);

			if (bytesRead > 0)
			{
				requestText.append(buffer.data(), bytesRead);
				headerEnd = requestText.find("\r\n\r\n");
			}
		} while (headerEnd == string::npos);
	}

	if (headerEnd == string::npos)
		throw RequestException("Header doesn't end");

	if (regex_search(requestText, requestLineMatch, requestLineFormat))
	{
		mMethod = requestLineMatch[1];
		mResource = requestLineMatch[2];
		mVersion = requestLineMatch[3];

		if (regex_match(mResource, queryStringMatch, queryStringFormat))
		{
			for (std::sregex_iterator it(mResource.begin() + queryStringMatch.position(1), mResource.end(), queryStringParams), end; it != end; ++it)
				queryStringArguments[(*it)[1]] = (*it)[2];

			mResource.erase(mResource.begin() + queryStringMatch.position(1), mResource.end());
		}
	}
	else
		throw RequestException("Request line is malformed");

	for (std::sregex_iterator i(requestText.begin() + requestLineMatch[0].str().size(), requestText.end(), requestHeaderFormat), end; i != end; ++i)
		mFields[(*i)[1]] = (*i)[2];

	try {
		contentLength = std::stoul(mFields.at(getFieldText(HeaderField::ContentLength)));
	}
	catch (const std::invalid_argument&) {
		contentLength = 0;
	}
	catch (const std::out_of_range&) {
		contentLength = 0;
	}

	if (contentLength)
	{
		mBody.insert(mBody.begin(), requestText.begin() + headerEnd + 4, requestText.end());

		while (mBody.size() < contentLength)
		{
			if (tlsSocket)
			{
				auto aux = tlsSocket->receiveTLSMessage(flags);

				mBody.insert(mBody.end(), aux.begin(), aux.end());
			}
			else
			{
				auto bytesRead = mSock->receive(buffer.data(), buffer.size(), flags);

				if (bytesRead > 0)
					mBody.insert(mBody.end(), buffer.begin(), buffer.begin() + bytesRead);
			}
		}
	}
}

std::string_view Http::Request::Impl::getMethod()
{
	return mMethod;
}

std::string_view Http::Request::Impl::getResourcePath()
{
	return mResource;
}

std::string_view Http::Request::Impl::getVersion()
{
	return mVersion;
}

std::optional<std::string_view> Http::Request::Impl::getField(HeaderField field)
{
	try {
		return mFields.at(getFieldText(field));
	}
	catch (const std::out_of_range&) {
		return std::optional<std::string_view>();
	}
}

std::optional<std::string_view> Http::Request::Impl::getField(std::string_view field)
{
	try {
		return mFields.at(field.data());
	}
	catch (const std::out_of_range&) {
		return std::optional<std::string_view>();
	}
}

std::optional<std::string_view> Http::Request::Impl::getRequestStringValue(std::string_view key)
{
	try {
		return queryStringArguments.at(key.data());
	}
	catch (const std::out_of_range&) {
		return std::optional<std::string_view>();
	}
}

std::vector<std::string_view> Http::Request::Impl::getRequestStringKeys()
{
	std::vector<std::string_view> result;
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
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::Request::Request(const std::shared_ptr<Socket> &sockWrapper)
	:mThis(new Impl(sockWrapper))
{}

Http::Request::~Request() noexcept
{
	delete mThis;
}

Http::Request::Request(Request &&other) noexcept
	:mThis(other.mThis)
{
	other.mThis = nullptr;
}

Http::Request& Http::Request::operator=(Request &&other) noexcept
{
	delete mThis;
	mThis = other.mThis;
	other.mThis = nullptr;

	return *this;
}

std::string_view Http::Request::getMethod()
{
	return mThis->getMethod();
}

std::string_view Http::Request::getResourcePath()
{
	return mThis->getResourcePath();
}

std::string_view Http::Request::getVersion()
{
	return mThis->getVersion();
}

std::optional<std::string_view> Http::Request::getField(HeaderField field)
{
	return mThis->getField(field);
}

std::optional<std::string_view> Http::Request::getField(std::string_view field)
{
	return mThis->getField(field);
}

std::optional<std::string_view> Http::Request::getRequestStringValue(std::string_view key)
{
	return mThis->getRequestStringValue(key);
}

std::vector<std::string_view> Http::Request::getRequestStringKeys()
{
	return mThis->getRequestStringKeys();
}

const std::vector<std::uint8_t>& Http::Request::getBody()
{
	return mThis->getBody();
}