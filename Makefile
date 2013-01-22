.PHONY: clean

OPTS=--std=c11 -O3 -Wall
PTHREAD_LIBS=-lpthread
JACK_LIBS=`pkg-config --cflags --libs jack`
FFTW_LIBS=`pkg-config --cflags --libs fftw3f`

LIBS=${PTHREAD_LIBS}${JACK_LIBS}${FFTW_LIBS}

datawave: src/datawave.c
	gcc -o $@ ${OPTS} ${LIBS} $<

clean:
	rm datawave
