STD=--std=c++17
INC=-I include
MACROS=-D NDEBUG

example : example/example.cpp example/Handlers.cpp Http.a 
	g++ $(INC) $(STD) $(MACROS) -o example.out $^ -pthread -lstdc++fs

Http.a : HttpServer.o HttpRequestResponse.o ThreadPool.o Common.o RequestScheduler.o
	ar rcs $@ $^

HttpServer.o : Common.o RequestScheduler.o HttpRequestResponse.o include/ExportMacros.h include/HttpServer.h src/HttpServer.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/HttpServer.cpp

HttpRequestResponse.o : Common.o include/HttpRequest.h include/HttpResponse.h include/ExportMacros.h src/HttpRequestResponse.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/HttpRequestResponse.cpp

RequestScheduler.o : Common.o ThreadPool.o src/RequestScheduler.h src/RequestScheduler.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/RequestScheduler.cpp

Common.o : src/Common.h src/Common.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/Common.cpp

ThreadPool.o : src/ThreadPool.h src/ThreadPool.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/ThreadPool.cpp

clean :
	rm *.o example.out Http.a