STD=--std=c++17
INC=-iquote include
MACROS=-D NDEBUG -D USE_DLL

.PHONY: all clean

all: Http.a libHttp.so



Http.a: HttpServer.o HttpRequest.o HttpResponse.o
	ar rcs $@ $^

libHttp.so: HttpServer.o HttpRequest.o HttpResponse.o
	g++ $(INC) $(STD) $(MACROS) -shared -fvisibility=hidden -o $@ $^ -pthread



HttpServer.o: src/HttpServer.cpp src/Common.h ThreadPool.o Socket.o HttpRequest.o HttpResponse.o include/ExportMacros.h include/HttpServer.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

HttpRequest.o: src/HttpRequest.cpp src/Common.h Socket.o include/HttpRequest.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

HttpResponse.o: src/HttpResponse.cpp src/Common.h Socket.o include/HttpResponse.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

ThreadPool.o: src/ThreadPool.cpp src/ThreadPool.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

Socket.o: src/Socket.cpp src/Socket.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<



clean:
	@rm *.o Http.a libHttp.so