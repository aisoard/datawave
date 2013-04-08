.PHONY: all clean reset

CC=clang

CFLAGS+=-O3 -Wall -g

LDFLAGS+=-lpthread
LDFLAGS+=`pkg-config --cflags --libs jack`
LDFLAGS+=`pkg-config --cflags --libs fftw3f`

all: datawave | bin

bin/main.o: src/main.c src/common.h | bin
	$(COMPILE.c) $(OUTPUT_OPTION) $<

bin/tools.o: src/tools.c src/common.h | bin
	$(COMPILE.c) $(OUTPUT_OPTION) $<

bin/datawave.o: src/datawave.c src/common.h | bin
	$(COMPILE.c) $(OUTPUT_OPTION) $<

datawave: bin/main.o bin/tools.o bin/datawave.o | bin
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

bin:
	mkdir bin

clean:
	rm -f bin/main.o bin/tools.o bin/datawave.o

reset: clean
	rm datawave fftwf_wisdom
