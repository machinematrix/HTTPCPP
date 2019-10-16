#include <string>
#include <type_traits>
#include <sstream>
#include <map>
#include <array>
#include <limits>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Common.h"
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

class Http::HttpRequest::Impl
{
	WinsockLoader mLoader;
	std::string mMethod;
	std::string mResource;
	std::string mVersion;
	std::array<std::string, static_cast<size_t>(HeaderField::Warning) + 1u> mFields;
	std::vector<std::int8_t> mBody;
	DescriptorType mSock;
	Status mStatus;

	static const char* getFieldText(HeaderField field);
	HeaderField getFieldId(const std::string &field);
public:
	Impl(const SocketWrapper &mSock);

	std::string_view getMethod();
	std::string_view getResource();
	std::string_view getVersion();
	std::string_view getField(HeaderField field);
	const std::vector<std::int8_t>& getBody();
	Status getStatus();
	DescriptorType getSocket();
};

const char* Http::HttpRequest::Impl::getFieldText(HeaderField field)
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

Http::HttpRequest::HeaderField Http::HttpRequest::Impl::getFieldId(const std::string &field)
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

Http::HttpRequest::Impl::Impl(const SocketWrapper &sockWrapper)
	:mSock(sockWrapper.mSock)
	,mStatus(Status::EMPTY)
{
	using std::string;
	using std::istringstream;
	using std::array;

	#ifdef _WIN32
	int flags = 0;
	NonBlockSocket nonBlock(mSock);
	#elif defined(__linux__)
	int flags = MSG_DONTWAIT;
	#endif
	int chances = 30;
	istringstream requestStream;
	string requestText;
	array<string::value_type, 256> buffer;
	string::size_type headerEnd = string::npos;

	do
	{
		auto bytesRead = recv(mSock, buffer.data(), buffer.size(), flags);

		if (bytesRead != SOCKET_ERROR && bytesRead > 0)
		{
			requestText.append(buffer.data(), bytesRead);
			headerEnd = requestText.rfind("\r\n\r\n");
			chances = 30;
		}
		else
		{
			Sleep(25);
			--chances;
		}
	}
	while (chances && headerEnd == string::npos);

	if (requestText.empty()) {
		return;
	}

	requestStream.str(requestText);

	requestStream >> mMethod;
	requestStream >> mResource;
	requestStream >> mVersion;

	string::size_type versionBegin = mVersion.find_first_of("1234567890");
	if (versionBegin != string::npos) {
		mVersion.erase(mVersion.begin(), mVersion.begin() + versionBegin);
	}

	requestStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	for(string header; std::getline(requestStream, header) && header != "\r";)
	{
		string::size_type colon = header.find(':');
		
		if (colon != std::string::npos)
		{
			string::size_type carriageReturn = header.find('\r', colon);
			
			if (carriageReturn != string::npos)
			{
				HeaderField id = getFieldId(header.substr(0, colon));
				if (id != HeaderField::Invalid)
				{
					mFields[static_cast<size_t>(id)] = header.substr(colon + 1, carriageReturn - colon - 1);
				}
			}
			else {
				mStatus = Status::MALFORMED;
				return;
			}
		}
		else {
			mStatus = Status::MALFORMED;
			return;
		}
	}

	if (headerEnd != string::npos)
	{
		unsigned contentLength;

		try {
			contentLength = std::stoul(mFields.at(static_cast<decltype(mFields)::size_type>(HeaderField::ContentLength)));
		}
		catch (const std::invalid_argument&) {
			contentLength = 0;
		}

		if (contentLength)
		{
			mBody.insert(mBody.begin(), requestText.begin() + headerEnd + 4, requestText.end());
			chances = 30;

			while (mBody.size() < contentLength && chances)
			{
				auto bytesRead = recv(mSock, buffer.data(), buffer.size(), flags);

				if (bytesRead != SOCKET_ERROR && bytesRead > 0)
				{
					mBody.insert(mBody.end(), buffer.begin(), buffer.begin() + bytesRead);
					chances = 30;
				}
				else
				{
					Sleep(25);
					--chances;
				}
			}
		}
	}
	else {
		mStatus = Status::MALFORMED;
		return;
	}

	mStatus = Status::OK;
}

std::string_view Http::HttpRequest::Impl::getMethod()
{
	return mMethod;
}

std::string_view Http::HttpRequest::Impl::getResource()
{
	return mResource;
}

std::string_view Http::HttpRequest::Impl::getVersion()
{
	return mVersion;
}

std::string_view Http::HttpRequest::Impl::getField(HeaderField field)
{
	return mFields[static_cast<size_t>(field)];
}

const std::vector<std::int8_t>& Http::HttpRequest::Impl::getBody()
{
	return mBody;
}

Http::HttpRequest::Status Http::HttpRequest::Impl::getStatus()
{
	return mStatus;
}

DescriptorType Http::HttpRequest::Impl::getSocket()
{
	return mSock;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::HttpRequest::HttpRequest(const SocketWrapper &sockWrapper)
	:mThis(new Impl(sockWrapper))
{}

Http::HttpRequest::~HttpRequest() noexcept = default;

Http::HttpRequest::HttpRequest(HttpRequest&&) noexcept = default;

Http::HttpRequest& Http::HttpRequest::operator=(HttpRequest&&) noexcept = default;

std::string_view Http::HttpRequest::getMethod()
{
	return mThis->getMethod();
}

std::string_view Http::HttpRequest::getResource()
{
	return mThis->getResource();
}

std::string_view Http::HttpRequest::getVersion()
{
	return mThis->getVersion();
}

std::string_view Http::HttpRequest::getField(HeaderField field)
{
	return mThis->getField(field);
}

const std::vector<std::int8_t>& Http::HttpRequest::getBody()
{
	return mThis->getBody();
}

Http::HttpRequest::Status Http::HttpRequest::getStatus()
{
	return mThis->getStatus();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//RESPONSE-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class Http::HttpResponse::Impl
{
	WinsockLoader mLoader;
	std::map<HeaderField, std::string> mFields;
	std::string mVersion;
	std::vector<int8_t> mBody;
	DescriptorType mSock;
	std::uint16_t mStatusCode;

	static const char* getFieldText(HeaderField field);
public:
	Impl(const HttpRequest&);

	void setBody(const decltype(mBody)&);
	void setStatusCode(std::uint16_t);
	void setField(HeaderField field, const std::string &value);
	void send();
};

const char* Http::HttpResponse::Impl::getFieldText(HeaderField field)
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

Http::HttpResponse::Impl::Impl(const HttpRequest &request)
	:mSock(request.mThis->getSocket()),
	mStatusCode(0),
	mVersion("1.1")
{}

void Http::HttpResponse::Impl::setBody(const decltype(mBody) &newBody)
{
	mBody = newBody;
}

void Http::HttpResponse::Impl::setStatusCode(std::uint16_t code)
{
	mStatusCode = code;
}

void Http::HttpResponse::Impl::setField(HeaderField field, const std::string &value)
{
	mFields[field] = value;
}

void Http::HttpResponse::Impl::send()
{
	if (!mStatusCode)
		return; // TODO: Handle invalid status codes in HttpResponse::Impl::send

	const char *fieldEnd("\r\n");

	decltype(::send(DescriptorType(), nullptr, 0, 0)) bytesSent = 0;

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
		decltype(bytesSent) auxBytesSent = ::send(mSock, response.data() + bytesSent, response.size() - bytesSent, 0);

		if (auxBytesSent > 0)
			bytesSent += auxBytesSent;
		else {
			break;
			// TODO: Handle failed writes to socket
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Http::HttpResponse::HttpResponse(const HttpRequest &request)
	:mThis(new Impl(request))
{}

Http::HttpResponse::~HttpResponse() noexcept = default;

Http::HttpResponse::HttpResponse(HttpResponse &&) noexcept = default;

Http::HttpResponse& Http::HttpResponse::operator=(HttpResponse&&) noexcept = default;

void Http::HttpResponse::setBody(const std::vector<std::int8_t>& mBody)
{
	mThis->setBody(mBody);
}

void Http::HttpResponse::setStatusCode(std::uint16_t code)
{
	mThis->setStatusCode(code);
}

void Http::HttpResponse::setField(HeaderField field, const std::string &value)
{
	mThis->setField(field, value);
}

void Http::HttpResponse::send()
{
	mThis->send();
}