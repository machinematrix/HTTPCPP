STD=--std=c++17
INC=-I include
MACROS=-D NDEBUG -D DLL
OBJDIR = obj
BINDIR = bin
OBJFILES = $(OBJDIR)/HttpServer.o $(OBJDIR)/HttpRequest.o $(OBJDIR)/HttpResponse.o $(OBJDIR)/ThreadPool.o $(OBJDIR)/Common.o $(OBJDIR)/RequestScheduler.o

all : $(BINDIR)/Http.a $(BINDIR)/Http.so $(BINDIR)/exampleStatic.out $(BINDIR)/exampleDynamic.out



$(BINDIR)/Http.a : $(OBJFILES) $(BINDIR)
	ar rcs $@ $(OBJFILES)

$(BINDIR)/libHttp.so : $(OBJFILES) $(BINDIR)#HttpServer.o HttpRequest.o HttpResponse.o RequestScheduler.o Common.o ThreadPool.o
	g++ $(INC) $(STD) $(MACROS) -shared -fvisibility=hidden -o $@ $(OBJFILES) -pthread

$(BINDIR)/exampleStatic.out : example/example.cpp example/Handlers.cpp $(BINDIR)/Http.a
	g++ $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs

$(BINDIR)/exampleDynamic.out : example/example.cpp example/Handlers.cpp $(BINDIR)/libHttp.so
	g++ -L . $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs



$(OBJDIR)/HttpServer.o : src/HttpServer.cpp $(OBJDIR)/Common.o $(OBJDIR)/RequestScheduler.o $(OBJDIR)/HttpRequest.o $(OBJDIR)/HttpResponse.o include/ExportMacros.h include/HttpServer.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

$(OBJDIR)/HttpRequest.o : src/HttpRequest.cpp $(OBJDIR)/Common.o include/HttpRequest.h include/ExportMacros.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

$(OBJDIR)/HttpResponse.o : src/HttpResponse.cpp $(OBJDIR)/Common.o include/HttpResponse.h include/ExportMacros.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

$(OBJDIR)/RequestScheduler.o : src/RequestScheduler.cpp $(OBJDIR)/Common.o $(OBJDIR)/ThreadPool.o src/RequestScheduler.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

$(OBJDIR)/Common.o : src/Common.cpp src/Common.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<

$(OBJDIR)/ThreadPool.o : src/ThreadPool.cpp src/ThreadPool.h $(OBJDIR)
	g++ $(INC) $(STD) $(MACROS) -c -fPIC -fvisibility=hidden -o $@ $<



$(OBJDIR) :
	@mkdir -p $@

$(BINDIR) :
	@mkdir -p $@

.clean :
	@rm $(OBJDIR)/*.o exampleStatic.out exampleDynamic.out Http.a Http.so