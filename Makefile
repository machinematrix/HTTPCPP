STD=--std=c++17
INC=-I include
MACROS=-D NDEBUG

example.out : example/example.cpp example/Handlers.cpp Http.a 
	g++ $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs

Http.a : HttpServer.o HttpRequest.o HttpResponse.o ThreadPool.o Common.o RequestScheduler.o
	ar rcs $@ $^

HttpServer.o : src/HttpServer.cpp Common.o RequestScheduler.o HttpRequest.o HttpResponse.o include/ExportMacros.h include/HttpServer.h
	g++ $(INC) $(STD) $(MACROS) -c $<

HttpRequest.o : src/HttpRequest.cpp Common.o include/HttpRequest.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c $<

HttpResponse.o : src/HttpResponse.cpp Common.o include/HttpResponse.h include/ExportMacros.h
	g++ $(INC) $(STD) $(MACROS) -c $<

RequestScheduler.o : src/RequestScheduler.cpp Common.o ThreadPool.o src/RequestScheduler.h
	g++ $(INC) $(STD) $(MACROS) -c $<

Common.o : src/Common.cpp src/Common.h
	g++ $(INC) $(STD) $(MACROS) -c $<

ThreadPool.o : src/ThreadPool.cpp src/ThreadPool.h
	g++ $(INC) $(STD) $(MACROS) -c $<

clean :
	rm *.o example.out Http.a