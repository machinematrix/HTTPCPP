EXAMPLEDIR=HTTPCPP-Example
LIBRARYDIR=HTTPCPP

.PHONY: all library example clean

all: library example

library:
	make -C $(LIBRARYDIR)

example:
	make -C $(EXAMPLEDIR)

clean:
	@-make -C $(EXAMPLEDIR) clean
	@-make -C $(LIBRARYDIR) clean