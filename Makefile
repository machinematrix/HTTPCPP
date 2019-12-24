STD=--std=c++17
INC=-I include
MACROS=-D NDEBUG
OBJDIR = obj
BINDIR = bin
OBJFILES = $(OBJDIR)/HttpServer.o $(OBJDIR)/HttpRequest.o $(OBJDIR)/HttpResponse.o $(OBJDIR)/ThreadPool.o $(OBJDIR)/Common.o $(OBJDIR)/RequestScheduler.o

all : Http.a Http.so exampleStatic.out exampleDynamic.out



Http.a : $(OBJFILES)
	ar rcs $@ $^

Http.so : $(OBJFILES)
	g++ $(INC) $(STD) $(MACROS) -shared -o $@ $^ -pthread

exampleStatic.out : example/example.cpp example/Handlers.cpp Http.a 
	g++ $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs

exampleDynamic.out : example/example.cpp example/Handlers.cpp Http.so
	g++ -L . $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs



HttpServer.o : src/HttpServer.cpp Common.o RequestScheduler.o HttpRequest.o HttpResponse.o include/ExportMacros.h include/HttpServer.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<

HttpRequest.o : src/HttpRequest.cpp Common.o include/HttpRequest.h include/ExportMacros.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<

HttpResponse.o : src/HttpResponse.cpp Common.o include/HttpResponse.h include/ExportMacros.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<

RequestScheduler.o : src/RequestScheduler.cpp Common.o ThreadPool.o src/RequestScheduler.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<

Common.o : src/Common.cpp src/Common.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<

ThreadPool.o : src/ThreadPool.cpp src/ThreadPool.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -o $(OBJDIR)/$@ $<



$(OBJDIR) :
	mkdir -p $@

$(BINDIR) :
	mkdir -p $@

clean :
	rm *.o exampleStatic.out exampleDynamic.out Http.a Http.so