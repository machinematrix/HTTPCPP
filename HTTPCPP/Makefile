STD=--std=c++17
INC=-iquote include
MACROS=-D NDEBUG -D USE_DLL

.PHONY: all clean

all: Http.a libHttp.so



Http.a: HttpServer.o HttpRequest.o HttpResponse.o RequestScheduler.o Common.o ThreadPool.o
	ar rcs $@ $^

libHttp.so: HttpServer.o HttpRequest.o HttpResponse.o RequestScheduler.o Common.o ThreadPool.o
	g++ $(INC) $(STD) $(MACROS) -shared -fvisibility=hidden -o $@ $^ -pthread



HttpServer.o: src/HttpServer.cpp Common.o RequestScheduler.o HttpRequest.o HttpResponse.o include/ExportMacros.h include/HttpServer.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

HttpRequest.o: src/HttpRequest.cpp Common.o include/HttpRequest.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

HttpResponse.o: src/HttpResponse.cpp Common.o include/HttpResponse.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

RequestScheduler.o: src/RequestScheduler.cpp Common.o ThreadPool.o src/RequestScheduler.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

Common.o: src/Common.cpp src/Common.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

ThreadPool.o: src/ThreadPool.cpp src/ThreadPool.h
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<



clean:
	@rm *.o Http.a libHttp.so