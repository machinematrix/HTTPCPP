STD=--std=c++17
INC=-I include
MACROS=-D NDEBUG

example : HttpServer.o ThreadPool.o HttpRequestResponse.o Common.o RequestScheduler.o example/example.cpp example/Handlers.cpp
	g++ $(INC) $(STD) $(MACROS) -o example.out example/example.cpp example/Handlers.cpp HttpServer.o ThreadPool.o HttpRequestResponse.o Common.o RequestScheduler.o -pthread -lstdc++fs

HttpServer.o : include/HttpServer.h src/HttpServer.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/HttpServer.cpp

HttpRequestResponse.o : include/HttpRequest.h include/HttpResponse.h src/HttpRequestResponse.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/HttpRequestResponse.cpp

Common.o : src/Common.h src/Common.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/Common.cpp

ThreadPool.o : src/ThreadPool.h src/ThreadPool.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/ThreadPool.cpp

RequestScheduler.o : src/RequestScheduler.h src/RequestScheduler.cpp
	g++ $(INC) $(STD) $(MACROS) -c src/RequestScheduler.cpp

clean :
	rm *.o example.out