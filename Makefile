.PHONY: clean

PTHREAD_LIBS=-lpthread
JACK_LIBS=`pkg-config --cflags --libs jack`
FFTW_LIBS=`pkg-config --cflags --libs fftw3f`

LIBS=${PTHREAD_LIBS}${JACK_LIBS}${FFTW_LIBS}

datawave: src/datawave.c
	gcc -o $@ -g ${LIBS} $<

clean:
	rm datawave
