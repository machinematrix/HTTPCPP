#include <iostream>
#include <string>
#include <fstream>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace std
{
	wstring to_wstring(const string&);
	string to_string(const wstring&);
}

std::vector<std::wstring> filenames(const std::string&);
std::vector<std::wstring> getJpgs(const std::string&);
std::vector<char> loadFile(const std::string&);

void redirect(Http::Request &req)
{
	using Http::Response;
	Response resp(req);

	resp.setStatusCode(303);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::CacheControl, "no-store");
	resp.setField(Response::HeaderField::Location, "/list");
	resp.send();
}

void favicon(Http::Request &req)
{
	using Http::Response;
	Response resp(req);

	auto fileBytes = loadFile("favicon.ico");

	if (!fileBytes.empty()) {
		resp.setStatusCode(200);
		resp.setField(Response::HeaderField::ContentType, "image/x-icon");
		resp.setField(Response::HeaderField::ContentLength, std::to_string(fileBytes.size()).c_str());
		resp.setField(Response::HeaderField::Connection, "close");
		resp.setBody({ fileBytes.begin(), fileBytes.end() });
	}
	else
		resp.setStatusCode(404);

	resp.send();
}

void list(Http::Request &req)
{
	using Http::Response;
	auto pictures = getJpgs(".");
	Response resp(req);

	std::string mBody;
	for (const auto &name : getJpgs(".")) {
		std::string asciiName(std::to_string(name));
		mBody += "<a href=\"/" + asciiName + "\">" + asciiName + "</a><br />";
	}

	if (mBody.empty())
		mBody = "No hay imagenes...";

	resp.setStatusCode(200);
	resp.setField(Response::HeaderField::ContentType, "text/html");
	resp.setField(Response::HeaderField::ContentLength, std::to_string(mBody.size()).c_str());
	resp.setField(Response::HeaderField::CacheControl, "no-store");
	resp.setField(Response::HeaderField::Connection, "close");
	resp.setBody({ mBody.begin(), mBody.end() });

	resp.send();
}

void image(Http::Request &req)
{
	using Http::Response;
	auto fileBytes = loadFile(req.getResource().data() + 1);

	if (!fileBytes.empty())
	{
		Response resp(req);

		resp.setStatusCode(200);
		resp.setField(Response::HeaderField::ContentLength, std::to_string(fileBytes.size()).c_str());
		resp.setField(Response::HeaderField::ContentType, "image/jpeg");
		resp.setField(Response::HeaderField::CacheControl, "no-store");
		resp.setField(Response::HeaderField::Connection, "close");
		resp.setBody({ fileBytes.begin(), fileBytes.end() });

		resp.send();
	}
}