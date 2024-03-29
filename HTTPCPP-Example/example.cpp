#include "HttpServer.h"
#include <iostream>
#include <fstream>
#include <filesystem>

void redirect(Http::Request&, Http::Response&);
void favicon(Http::Request&, Http::Response&);
void list(Http::Request&, Http::Response&, std::string_view endpoint, std::string_view extension);
void image(Http::Request&, Http::Response&);
void video(Http::Request &req, Http::Response &resp);

std::vector<std::string> filenames(const std::string_view &directory)
{
	using std::filesystem::directory_iterator;

	std::vector<std::string> result;
	directory_iterator iterator(directory), end;

	while (iterator != end)
	{
		result.push_back(iterator->path().filename().string());
		++iterator;
	}

	return result;
}

std::vector<std::string> getFilesWithExtension(std::string_view directory, std::string_view extension)
{
	std::vector<std::string> result(filenames(directory));

	for (auto i = result.begin(); i != result.end();)
	{
		auto ext = i->find(extension.data());

		if (ext != std::wstring::npos && ext == i->size() - 4)
			++i;
		else
			i = result.erase(i);
	}

	return result;
}

std::vector<std::uint8_t> loadFile(const std::string_view &fileName)
{
	std::vector<std::uint8_t> result;
	std::ifstream file(fileName.data(), std::ios::binary);

	if (file.is_open())
	{
		size_t size = std::filesystem::directory_entry{ fileName }.file_size();
		std::streamsize bytesRead = 0;
		result.resize(size);

		while (file) {
			file.read((char*)(result.data()) + bytesRead, 512);
			bytesRead += file.gcount();
		}
	}

	return result;
}

//range overflows file
std::vector<std::uint8_t> loadFile(const std::string_view &fileName, std::streampos first, std::streampos last)
{
	std::vector<std::uint8_t> result;

	if (first < last)
	{
		std::ifstream file(fileName.data(), std::ios::binary);

		if (file.is_open())
		{
			std::streamsize bytesRead = 0;
			result.resize(last - first);
			file.seekg(first);

			while (bytesRead < last - first) {
				file.read((char*)(result.data()) + bytesRead, last - first - bytesRead);
				bytesRead += file.gcount();
			}
		}
	}

	return result;
}

void logger(std::string_view msg)
{
	std::cout << "Endpoint logger: " << msg << std::endl;
}

void errorLogger(std::string_view msg)
{
	std::cerr << "Error: " << msg << std::endl;
}

int main()
{
	using std::cout;
	using std::endl;
	using std::cin;

	try
	{
		std::string input;
		Http::Server sv(80, 443, 50, "MY", "localhost"); //You'll need a server authentication certificate named 'localhost' in the personal certificate store for this to work.

		sv.setResourceCallback("/images", std::bind(list, std::placeholders::_1, std::placeholders::_2, "image", ".jpg"));
		sv.setResourceCallback("/videos", std::bind(list, std::placeholders::_1, std::placeholders::_2, "video", ".mp4"));
		sv.setResourceCallback("/", redirect);
		sv.setResourceCallback("/favicon.ico", favicon);
		sv.setResourceCallback("/image", image);
		sv.setResourceCallback("/video", video);
		sv.setEndpointLogger(logger);
		sv.setErrorLogger(errorLogger);
		sv.start();

		do
		{
			cin >> input;
			if (input == "clear")
			{
				#ifdef _WIN32
				system("cls");
				#elif defined __linux__
				system("clear");
				#endif
			}
		} while (input != "exit" && cin);
	}
	catch (const std::runtime_error &e)
	{
		cout << e.what() << endl;
	}

	return 0;
}