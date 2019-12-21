#include <iostream>
#include <string>
#include <fstream>
#include "HttpResponse.h"
#include "HttpRequest.h"

std::vector<std::string> getJpgs(const std::string&);
std::vector<std::uint8_t> loadFile(const std::string&);

using Http::Response;
using Http::Request;

namespace
{
	void sendNotAllowed(Http::Request &req, const std::string &allowedMethods)
	{
		using Http::Response;
		Response resp(req);

		resp.setStatusCode(405);
		resp.setField(Response::HeaderField::Connection, "close");
		resp.setField(Response::HeaderField::CacheControl, "no-store");
		resp.setField(Response::HeaderField::Allow, allowedMethods);
		resp.send();
	}

	void setKeepAlive(Http::Request &request, Http::Response &response)
	{
		if (request.getField(Http::Request::HeaderField::Connection) == "keep-alive")
			response.setField(Response::HeaderField::Connection, "keep-alive");
		else {
			response.setField(Response::HeaderField::Connection, "close");
			request.toggleKeepAlive(false);
		}
	}
}

void redirect(Http::Request &req)
{
	Response resp(req);

	resp.setStatusCode(303);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::CacheControl, "no-store");

	setKeepAlive(req, resp);

	resp.setField(Response::HeaderField::Location, "/list");
	resp.send();
}

void favicon(Request &req)
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, "GET");
		return;
	}

	Response resp(req);

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

void list(Request &req)
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, "GET");
		return;
	}

	auto pictures = getJpgs(".");
	Response resp(req);

	std::string mBody;
	for (const auto &name : getJpgs(".")) {
		mBody += "<a href=\"/image?name=" + name + "\">" + name + "</a><br />";
	}

	if (mBody.empty())
		mBody = "No images...";

	resp.setStatusCode(200);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::ContentLength, std::to_string(mBody.size()));
	resp.setField(Response::HeaderField::CacheControl, "no-store");

	setKeepAlive(req, resp);

	resp.setBody(mBody);

	resp.send();
}

void image(Request &req) //?name=<image file name>
{
	if (req.getMethod() != "GET") {
		sendNotAllowed(req, "GET");
		return;
	}

	Response resp(req);
	auto name = req.getRequestStringValue("name");

	if (name.find_first_of("\\/") == std::string::npos)
	{
		auto fileBytes = loadFile(name);

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