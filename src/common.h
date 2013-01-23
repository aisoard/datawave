#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#include <pthread.h>
#include <jack/jack.h>

jack_client_t * client;
jack_port_t * input_port_left;
jack_port_t * input_port_right;
jack_port_t * output_port_left;
jack_port_t * output_port_right;

#include <time.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#define N 65536 // Buffer size
#define G_IN 1.0f // Input gain
#define G_OUT 1.0f // Output gain
#define TAU 6.28318530717958647692f

#define MAX(x,y) (x > y ? x : y)

/* Current phase inside buffers */
int phase;

/* Tools: time/frequency domain */
void * wave;
float * wave_time;
fftwf_complex * wave_freq;
fftwf_plan fft_wave_time_to_freq;
fftwf_plan fft_wave_freq_to_time;

/* Input and Output: sound and data time domain */
float * in_sound_time;
float * in_data_time;
float * out_data_time;
float * out_sound_time;
fftwf_plan fft_out_sound_from_wave;

/* Impulse: time, frequency and autocorrelation domain */
float * impulse_time;
fftwf_complex * impulse_freq;
float * impulse_auto;
fftwf_plan fft_impulse_time_to_freq;
fftwf_plan fft_impulse_freq_to_time;

/* Client callbacks */
void init(void * arg);
int exec(jack_nframes_t n, void * arg);
void fini(void * arg);

/* Math tools */
float filter(float s, float p, float x);
float sine(float l, float x);
float gauss(float a, float x);
float noise(float min, float max);
void convolution(float * input_time, fftwf_complex * kernel_freq, float * output_time);
void correlation(float * input_time, fftwf_complex * kernel_freq, float * output_time);
void autocorrelation(float * data_time, float * data_auto);
void pulse(float * data_time);
void amplify(float * data_time, float a);
void normalize(float * data_time);

#endif // COMMON_H
