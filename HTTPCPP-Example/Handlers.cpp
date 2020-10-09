#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include "HttpResponse.h"
#include "HttpRequest.h"

std::vector<std::string> getFilesWithExtension(std::string_view, std::string_view);
std::streampos getFileSize(const std::string_view&);
std::streampos getFileSize(std::ifstream&);
std::vector<std::uint8_t> loadFile(const std::string_view&);
std::vector<std::uint8_t> loadFile(const std::string_view&, std::streampos, std::streampos);

using Http::Response;
using Http::Request;

namespace
{
	void sendNotAllowed(Request &req, Response &resp, const std::string &allowedMethods)
	{
		resp.setStatusCode(405);
		resp.setField(Response::HeaderField::Connection, "close");
		resp.setField(Response::HeaderField::CacheControl, "no-store");
		resp.setField(Response::HeaderField::Allow, allowedMethods);
		resp.send();
	}

	void setKeepAlive(Request &request, Response &response)
	{
		auto connectionHeader = request.getField("connection");

		if (connectionHeader && connectionHeader.value() == "keep-alive")
			response.setField(Response::HeaderField::Connection, "keep-alive");
		else
			response.setField(Response::HeaderField::Connection, "close");
	}
}

void redirect(Request &req, Response &resp)
{
	resp.setStatusCode(303);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::CacheControl, "no-store");

	setKeepAlive(req, resp);

	resp.setField(Response::HeaderField::Location, "/images");
	resp.send();
}

void favicon(Request &req, Response &resp)
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, resp, "GET");
		return;
	}

	auto fileBytes = loadFile("favicon.ico");

	if (!fileBytes.empty()) {
		resp.setStatusCode(200);
		resp.setField(Response::HeaderField::ContentType, "image/x-icon");
		resp.setField(Response::HeaderField::ContentLength, std::to_string(fileBytes.size()));

		setKeepAlive(req, resp);

		resp.setBody(fileBytes);
	}
	else
		resp.setStatusCode(404);

	resp.send();
}

void list(Request &req, Response &resp, std::string_view endpoint, std::string_view extension)
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, resp, "GET");
		return;
	}

	std::string mBody;

	for (const auto &name : getFilesWithExtension(".", /*".jpg"*/extension)) {
		//mBody += "<a href=\"/image?name=" + name + "\">" + name + "</a><br />";
		mBody += "<a href=\"/";
		mBody += endpoint;
		mBody += "?name=" + name + "\">" + name + "</a><br />";
	}

	if (mBody.empty())
		mBody = "Empty...";

	resp.setStatusCode(200);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::ContentLength, std::to_string(mBody.size()));
	resp.setField(Response::HeaderField::CacheControl, "no-store");

	setKeepAlive(req, resp);

	resp.setBody(mBody);

	resp.send();
}

void image(Request &req, Response &resp) //?name=<image file name>
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, resp, "GET");
		return;
	}

	auto name = req.getRequestStringValue("name");

	if (name.has_value() && name.value().find_first_of("\\/") == std::string::npos)
	{
		auto fileBytes = loadFile(name.value());

		if (!fileBytes.empty())
		{
			resp.setStatusCode(200);
			resp.setField(Response::HeaderField::ContentLength, std::to_string(fileBytes.size()));
			resp.setField(Response::HeaderField::ContentType, "image/jpeg");
			resp.setField(Response::HeaderField::CacheControl, "no-store");

			setKeepAlive(req, resp);

			resp.setBody(fileBytes);
		}
		else {
			resp.setStatusCode(422);
			resp.setField(Response::HeaderField::Connection, "close");
		}
	}
	else
	{
		resp.setStatusCode(422);
		resp.setField(Response::HeaderField::Connection, "close");
	}

	resp.send();
}

void video(Request &req, Response &resp) //?name=<video file name>
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, resp, "GET");
		#ifndef NDEBUG
		std::cout << "Request was not GET, it was " << req.getMethod() << std::endl;
		#endif
		return;
	}

	auto name = req.getRequestStringValue("name");

	if (name && name.value().find_first_of("\\/") == std::string::npos)
	{
		static std::regex rangeFormat("bytes=([[:digit:]]+)-([[:digit:]]*)");
		std::cmatch results;
		std::vector<std::uint8_t> chunk;
		std::size_t rangeBegin = 0, rangeEnd = 0, fileSize = getFileSize(name.value());
		auto range = req.getField(Request::HeaderField::Range);

		resp.setField(Response::HeaderField::ContentType, "video/mp4");
		resp.setField(Response::HeaderField::CacheControl, "no-store");
		resp.setField(Response::HeaderField::AcceptRanges, "bytes");

		setKeepAlive(req, resp);

		if (range && std::regex_match(range.value().data(), results, rangeFormat))
		{
			rangeBegin = std::stoull(results[1]);
			rangeEnd = (results[2].str().empty() ? std::min<decltype(chunk)::size_type>(rangeBegin + 1024 * 1024, fileSize) : std::stoull(results[2]));

			if (rangeBegin < rangeEnd && rangeEnd <= fileSize)
			{
				chunk = loadFile(name.value(), rangeBegin, rangeEnd);

				resp.setField(Response::HeaderField::ContentRange, "bytes " + std::to_string(rangeBegin) + '-' + std::to_string(rangeEnd - 1) + '/' + std::to_string(fileSize));
				resp.setField(Response::HeaderField::ContentLength, std::to_string(chunk.size()));
				resp.setBody(chunk);
				resp.setStatusCode(206);
			}
			else {
				resp.setStatusCode(416);
			}
		}
		else {
			std::uint64_t sent = 0ull, fileSize = getFileSize(name.value());

			resp.setStatusCode(200);
			resp.setField(Response::HeaderField::ContentLength, std::to_string(fileSize));
			resp.sendHeaders();

			while (sent < fileSize)
			{
				chunk = loadFile(name.value(), sent, std::min(fileSize, sent + 1024ull * 1024ull * 4ull));
				resp.sendBytes(chunk);
				sent += chunk.size();
			}

			return;
		}
	}
	else
	{
		resp.setStatusCode(422);
		resp.setField("connection", "close");
	}

	resp.send();
}