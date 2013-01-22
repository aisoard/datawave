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
jack_port_t * input_port;
jack_port_t * output_port;

#include <math.h>
#include <complex.h>
#include <fftw3.h>

#define N 65536 // Buffer size
#define G_IN 1.0f // Gain input
#define G_OUT 1.0f // Gain output

/* Current phase inside buffers */
int phase;

/* Input sound: time and frequency domain */
float * in_sound_time;
fftwf_complex * in_sound_freq;
fftwf_plan fft_in_sound;

/* Impulse sound to data: time and frequency domain */
float * impulse_time;
fftwf_complex * impulse_freq;
fftwf_plan fft_impulse;

/* Input data: time and frequency domain */
fftwf_complex * in_data_freq;
float * in_data_time;
fftwf_plan fft_in_data;

/* Output data: time and frequency domain */
float * out_data_time;
fftwf_complex * out_data_freq;
fftwf_plan fft_out_data;

/* Outpulse data to sound: time and frequency domain */
float * outpulse_time;
fftwf_complex * outpulse_freq;
fftwf_plan fft_outpulse;

/* Output sound: time and frequency domain */
fftwf_complex * out_sound_freq;
float * out_sound_time;
fftwf_plan fft_out_sound;

/* Client callbacks */
void init(void * arg);
int exec(jack_nframes_t n, void * arg);
void fini(void * arg);

/* Math tools */
float sine(float l, float x);
float gauss(float a, float x);
float noise(float min, float max);
void pulse(float * data_time);
void amplify(float * data_time, float a);
void normalize(float * data_time);

#endif // COMMON_H
