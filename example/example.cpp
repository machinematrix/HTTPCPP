#include "HttpServer.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace FilesystemNamespace = std::filesystem;

void redirect(Http::Request&);
void favicon(Http::Request&);
void list(Http::Request&);
void image(Http::Request&);
void kill(Http::Request&);

std::vector<std::string> filenames(const std::string &directory)
{
	using FilesystemNamespace::directory_iterator;

	std::vector<std::string> result;
	directory_iterator iterator(directory), end;

	while (iterator != end) {
		result.push_back(iterator->path().filename().string());
		++iterator;
	}

	return result;
}

std::vector<std::string> getJpgs(const std::string &directory)
{
	std::vector<std::string> result(filenames(directory));

	for (auto i = result.begin(); i != result.end();)
	{
		auto extension = i->find(".jpg");
		if (extension != std::wstring::npos && extension == i->size() - 4)
			++i;
		else
			i = result.erase(i);
	}

	return result;
}

std::vector<std::uint8_t> loadFile(const std::string &fileName)
{
	std::vector<std::uint8_t> result;
	std::streampos size;
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	if (file.is_open())
	{
		size = file.tellg();
		file.close();
		file.open(fileName, std::ios::binary);

		if (file.is_open())
		{
			std::streamsize bytesRead = 0;
			result.resize(size);

			while (file) {
				file.read((char*)(result.data()) + bytesRead, 512);
				bytesRead += file.gcount();
			}
		}
	}

	return result;
}

void logger(const std::string &msg)
{
	std::cout << msg << std::endl;
}

int main()
{
	using std::cout;
	using std::endl;
	using std::cin;

	try {
		std::string input;
		Http::Server sv(80); //you'll need root privileges to start a server on ports under 1024

		/*for (const auto &fileName : getJpgs(".")) {
			sv.setResourceCallback(("/" + std::to_string(fileName)).c_str(), image);
		}*/

		sv.setResourceCallback("/image", image);
		sv.setResourceCallback("/list", list);
		sv.setResourceCallback("/", redirect);
		sv.setResourceCallback("/favicon.ico", favicon);
		sv.setLogger(logger);
		sv.start();

		do
		{
			cin >> input;
			if (input == "clear")
			{
				#ifdef _WIN32
				system("cls");
				#elif defined
				system("clear");
				#endif
			}
		} while (input != "exit" && cin);
	}
	catch (const Http::ServerException &e) {
		cout << e.what() << endl;
	}

	return 0;
}