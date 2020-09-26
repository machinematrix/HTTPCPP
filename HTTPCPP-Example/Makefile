STD=--std=c++17
LIBRARYDIR=../HTTPCPP
INC=-iquote $(LIBRARYDIR)/include
MACROS=-D NDEBUG -D USE_DLL

.PHONY: all clean

all: exampleStatic.out exampleDynamic.out

exampleStatic.out: example.cpp Handlers.cpp $(LIBRARYDIR)/Http.a
	make -C $(LIBRARYDIR) Http.a
	g++ $(INC) $(STD) $(MACROS) -o $@ $^ -pthread -lstdc++fs

exampleDynamic.out: example.cpp Handlers.cpp $(LIBRARYDIR)/libHttp.so
	make -C $(LIBRARYDIR) libHttp.so
	g++ -L $(LIBRARYDIR) $(INC) $(STD) $(MACROS) -o $@ example.cpp Handlers.cpp -l Http -pthread -l stdc++fs

clean:
	@-rm exampleStatic.out exampleDynamic.out