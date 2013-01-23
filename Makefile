.PHONY: all clean reset

OPTS=--std=c11 -O3 -Wall -g
PTHREAD_LIBS=-lpthread
JACK_LIBS=`pkg-config --cflags --libs jack`
FFTW_LIBS=`pkg-config --cflags --libs fftw3f`

LIBS=${PTHREAD_LIBS}${JACK_LIBS}${FFTW_LIBS}

all: datawave | bin

bin/main.o: src/main.c src/common.h | bin
	gcc -c -o $@ ${OPTS} $<

bin/tools.o: src/tools.c src/common.h | bin
	gcc -c -o $@ ${OPTS} $<

bin/datawave.o: src/datawave.c src/common.h | bin
	gcc -c -o $@ ${OPTS} $<

datawave: bin/main.o bin/tools.o bin/datawave.o | bin
	gcc -o $@ ${OPTS} ${LIBS} $^

bin:
	mkdir bin

clean:
	rm -f bin/main.o bin/tools.o bin/datawave.o

reset: clean
	rm datawave fftwf_wisdom
