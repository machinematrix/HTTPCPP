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
		auto connectionHeader = request.getField(/*Http::Request::HeaderField::Connection*/"connection");

		if (connectionHeader || connectionHeader.value() == "keep-alive")
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
		return;
	}

	auto name = req.getRequestStringValue("name");

	if (name && name.value().find_first_of("\\/") == std::string::npos)
	{
		static std::regex rangeFormat("bytes=([[:digit:]]+)-([[:digit:]]*)");
		std::smatch results;
		std::string range;
		std::vector<std::uint8_t> chunk;
		std::size_t rangeBegin = 0, rangeEnd = 0, fileSize = getFileSize(name.value());

		range = req.getField(Request::HeaderField::Range).value_or("bytes=0-");
		resp.setField(Response::HeaderField::ContentType, "video/mp4");
		resp.setField(Response::HeaderField::CacheControl, "no-store");

		setKeepAlive(req, resp);

		if (std::regex_match(range, results, rangeFormat))
		{
			rangeBegin = std::stoull(results[1]);
			rangeEnd = (results[2].str().empty() ? std::min<decltype(chunk)::size_type>(rangeBegin + 1024 * 1024, fileSize) : std::stoull(results[2]));

			if (rangeBegin < rangeEnd && rangeEnd <= fileSize) {
				chunk = loadFile(name.value(), rangeBegin, rangeEnd);

				resp.setField(Response::HeaderField::ContentRange, "bytes " + std::to_string(rangeBegin) + '-' + std::to_string(rangeEnd - 1) + '/' + std::to_string(fileSize));
				resp.setField(Response::HeaderField::ContentLength, std::to_string(chunk.size()));
				resp.setBody(chunk);
				resp.setStatusCode(206);
				#ifndef NDEBUG
				std::cout << "Content-Range: " << resp.getField("content-range").value() << std::endl;
				#endif
			}
			else {
				resp.setStatusCode(416);
				#ifndef NDEBUG
				std::cout << "Sending request unsatisfiable" << std::endl;
				#endif
			}

			#ifndef NDEBUG
			std::cout << range << std::endl;
			#endif
		}
		else {
			resp.setStatusCode(200);
			resp.setField(Response::HeaderField::AcceptRanges, "bytes");
			#ifndef NDEBUG
			std::cout << "Sending AcceptRanges" << std::endl;
			#endif
			//rangeEnd = std::min<size_t>(1024 * 1024, fileSize);
		}
	}
	else
	{
		resp.setStatusCode(422);
		resp.setField("connection", "close");
	}

	resp.send();
}

//void video(Request &req, Response &resp)
//{
//	if (req.getMethod() != "GET") {
//		sendNotAllowed(req, resp, "GET");
//		return;
//	}
//	//static auto fileBytes = loadFile("B:\\Videos\\Battlefield 4\\Battlefield 4 07.22.2017 - 22.34.51.01.mp4");
//	static auto fileBytes = loadFile("B:\\Videos\\PCSX2\\PCSX2 2019.06.02 - 03.28.30.25.mp4");
//
//	if (!fileBytes.empty())
//	{
//		static std::regex rangeFormat("bytes=([[:digit:]]+)-([[:digit:]]*)");
//		std::smatch results;
//		std::string range;
//
//		range = req.getField(Request::HeaderField::Range);
//		resp.setField(Response::HeaderField::ContentType, "video/mp4");
//		resp.setField(Response::HeaderField::CacheControl, "no-store");
//		resp.setField(Response::HeaderField::AcceptRanges, "bytes");
//
//		setKeepAlive(req, resp);
//
//		if (std::regex_match(range, results, rangeFormat))
//		{
//			std::string contentRange("bytes "), fileSize(std::to_string(fileBytes.size()));
//			auto rangeBegin = std::stoull(results[1]), rangeEnd = (results[2].str().empty() ? std::min<decltype(fileBytes)::size_type>(rangeBegin + 1024 * 1024, fileBytes.size() - 1) : std::stoul(results[2]));
//
//			if (rangeEnd < fileBytes.size())
//			{
//				decltype(fileBytes) chunk(fileBytes.begin() + rangeBegin, fileBytes.begin() + rangeEnd + 1);
//
//				contentRange.append(results[1]);
//				contentRange.push_back('-');
//				contentRange.append(std::to_string(rangeEnd));
//				contentRange.push_back('/');
//				contentRange.append(fileSize);
//
//				/*std::cout << "Range: " << range << std::endl;
//				std::cout << "Content-Range: " << contentRange << std::endl;*/
//
//				resp.setBody(chunk);
//				resp.setStatusCode(206);
//				resp.setField(Response::HeaderField::ContentRange, contentRange);
//				resp.setField(Response::HeaderField::ContentLength, std::to_string(chunk.size()));
//			}
//			else
//				resp.setStatusCode(416);
//		}
//		else {
//			constexpr decltype(fileBytes)::size_type chunkSize = 1024 * 1024;
//			/*resp.setStatusCode(200);
//			resp.setField(Response::HeaderField::ContentLength, std::to_string(fileBytes.size()));
//			resp.setBody(fileBytes);*/
//			std::string contentRange = "bytes 0-";
//
//			contentRange.append(std::to_string(chunkSize - 1));
//			contentRange.push_back('/');
//			contentRange.append(std::to_string(fileBytes.size()));
//
//			resp.setStatusCode(206);
//			resp.setField(Response::HeaderField::ContentRange, contentRange);
//			//resp.setField(Response::HeaderField::ContentLength, std::to_string(chunk.size()));
//
//			std::cout << "sending full file" << std::endl;
//		}
//	}
//	else {
//		resp.setStatusCode(422);
//		resp.setField(Response::HeaderField::Connection, "close");
//	}
//
//	resp.send();
//}