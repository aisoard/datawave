#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#include <pthread.h>
#include <jack/jack.h>

#include <math.h>
#include <complex.h>
#include <fftw3.h>

#define N 65536 // Buffer
#define G 1.0f // Gain
#define TAU 6.28318530717958647692f // Math standard constant

jack_client_t * client;
jack_port_t * input_port_front;
jack_port_t * input_port_back;
jack_port_t * output_port_left;
jack_port_t * output_port_right;

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

void normalize(float * data_time)
{
	int i;

	float temp;
	float temp2;
	float max2 = 0;

	for(i = 0; i < N; i++) {
		temp = data_time[i];
		temp2 = temp * temp;
		max2 = max2 > temp2 ? max2 : temp2;
	}

	float factor = 1 / sqrt(max2);

	factor = factor;

	for(i = 0; i < N; i++)
		data_time[i] *= factor;
}

float gauss(float x)
{
	return expf(- (x*x) / (64*64));
}

float pulse(float x)
{
	return (((float) rand())/((float) RAND_MAX)*2-1)*gauss(x);
}

void analyse(void)
{
	//memcpy(out_data_time, in_data_time, sizeof(float) * N);
	memset(out_data_time, 0, sizeof(float) * N);
	out_data_time[0] = 1.0f;
}

void init(void * arg)
{
	phase = 0;

	/* Init input sound buffers and transform */
	in_sound_time = fftwf_alloc_real(N);
	in_sound_freq = fftwf_alloc_complex(N/2+1);
	fft_in_sound = fftwf_plan_dft_r2c_1d(N, in_sound_time, in_sound_freq, FFTW_MEASURE);

	/* Init impulse buffers and transform */
	impulse_time = fftwf_alloc_real(N);
	impulse_freq = fftwf_alloc_complex(N/2+1);
	fft_impulse = fftwf_plan_dft_r2c_1d(N, impulse_time, impulse_freq, FFTW_MEASURE);

	/* Init input data buffers and transform */
	in_data_freq = fftwf_alloc_complex(N/2+1);
	in_data_time = fftwf_alloc_real(N);
	fft_in_data = fftwf_plan_dft_c2r_1d(N, in_data_freq, in_data_time, FFTW_MEASURE);

	/* Init output data buffers and transform */
	out_data_time = fftwf_alloc_real(N);
	out_data_freq = fftwf_alloc_complex(N/2+1);
	fft_out_data = fftwf_plan_dft_r2c_1d(N, out_data_time, out_data_freq, FFTW_MEASURE);

	/* Init outpulse buffers and transform */
	outpulse_time = fftwf_alloc_real(N);
	outpulse_freq = fftwf_alloc_complex(N/2+1);
	fft_outpulse = fftwf_plan_dft_r2c_1d(N, outpulse_time, outpulse_freq, FFTW_MEASURE);

	/* Init output sound buffers and transform */
	out_sound_freq = fftwf_alloc_complex(N/2+1);
	out_sound_time = fftwf_alloc_real(N);
	fft_out_sound = fftwf_plan_dft_c2r_1d(N, out_sound_freq, out_sound_time, FFTW_MEASURE);

	/* Pre-compute impulse and outpulse frequency domain */
	for(int i = 0; i < N; i++)
		outpulse_time[i] = pulse(i) + pulse(i-N);
	normalize(outpulse_time);
	fftwf_execute(fft_outpulse);
	for(int i = 0; i < N/2+1; i++)
		impulse_freq[i] = 1/outpulse_freq[i];

	for(int i = 0; i < N/2+1; i++) {
		outpulse_freq[i] *= G/N;
		impulse_freq[i] *= G/N;
	}
}

void fini(void * arg)
{
	/* Free input sound buffers and transform */
	fftwf_destroy_plan(fft_in_sound);
	fftwf_free(in_sound_time);
	fftwf_free(in_sound_freq);

	/* Free impulse buffers and transform */
	fftwf_destroy_plan(fft_impulse);
	fftwf_free(impulse_time);
	fftwf_free(impulse_freq);

	/* Free input data buffers and transform */
	fftwf_destroy_plan(fft_in_data);
	fftwf_free(in_data_freq);
	fftwf_free(in_data_time);

	/* Free output data buffers and transform */
	fftwf_destroy_plan(fft_out_data);
	fftwf_free(out_data_time);
	fftwf_free(out_data_freq);

	/* Free outpulse buffers and transform */
	fftwf_destroy_plan(fft_outpulse);
	fftwf_free(outpulse_time);
	fftwf_free(outpulse_freq);

	/* Free output sound buffers and transform */
	fftwf_destroy_plan(fft_out_sound);
	fftwf_free(out_sound_freq);
	fftwf_free(out_sound_time);

	exit(0);
}

int exec(jack_nframes_t n, void * arg)
{
	int i;

	/* Connect both input and both output */
	jack_default_audio_sample_t * in_front = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port_front, n);
	jack_default_audio_sample_t * in_back = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port_back, n);
	jack_default_audio_sample_t * out_left = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_left, n);
	jack_default_audio_sample_t * out_right = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_right, n);

	/* Retrive input data at the current phase and send output data at the opposed phase
	 * Suppose N multiple of n
	 */
	for(i = 0; i < n; i++) {
		int phase_in = phase + i;
		int phase_out = (phase + i + N/2) % N;

		in_sound_time[phase_in] = (in_front[i] + in_back[i])/2;
		out_left[i] = out_sound_time[phase_out];
		out_right[i] = out_sound_time[phase_out];
	}

	/* Update phase */
	phase += n;
	phase %= N;

	/* Update input sound frequency domain */
	fftwf_execute(fft_in_sound);

	/* Compute input data frequency domain (convolution) */
	for(i = 0; i < N/2+1; i++)
		in_data_freq[i] = in_sound_freq[i] * impulse_freq[i];

	/* Update input data time domain */
	fftwf_execute(fft_in_data);

	/* Analyse in data and generate out data */
	analyse();

	/* Update output data frequency domain */
	fftwf_execute(fft_out_data);

	/* Compute output sound frequency domain (convolution) */
	for(i = 0; i < N/2+1; i++)
		out_sound_freq[i] = out_data_freq[i] * outpulse_freq[i];

	/* Update output sound time domain */
	fftwf_execute(fft_out_sound);

	//memcpy(out_sound_time, in_data_time, sizeof(float) * N); // FIXME debug

	return 0;
}

static void signal_handler(int signal)
{
	jack_client_close(client);
	errx(0, "signal received, exiting ...\n");
}

int main(int argc, char * argv[])
{
	const char ** ports = NULL;
	const char * client_name = NULL;
	const char * server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	/* Retrieve options */
	client_name = strrchr(argv[0], '/');
	if(client_name == 0) client_name = argv[0];
	else client_name++;

	if(argc >= 2)
		client_name = argv[1];

	if(argc >= 3) {
		server_name = argv[2];
		options |= JackServerName;
	}

	/* Register client */
	client = jack_client_open(client_name, options, &status, server_name);
	if(client == NULL) {
		if(status & JackServerFailed)
			warnx("Unable to connect to JACK server");
		errx(1, "jack_client_open() failed, status = 0x%2.0x", status);
	}

	if(status & JackServerStarted)
		warnx("JACK server started");

	if(status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		warnx("name already assigned, client renamed to `%s'", client_name);
	}

	/* Register callbacks */
	init(NULL);
	//jack_set_thread_init_callback(client, init, NULL);
	jack_set_process_callback(client, exec, NULL);
	jack_on_shutdown(client, fini, NULL);

	/* Display server caracteristics */
	printf("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate(client));

	/* Register input ports */
	input_port_front = jack_port_register(client, "input front", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if(input_port_front == NULL)
		errx(1, "no more JACK input ports available");
	input_port_back = jack_port_register(client, "input back", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if(input_port_back == NULL)
		errx(1, "no more JACK input ports available");

	/* Register output ports */
	output_port_left = jack_port_register(client, "output left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if(output_port_left == NULL)
		errx(1, "no more JACK output ports available");
	output_port_right = jack_port_register(client, "output right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if(output_port_right == NULL)
		errx(1, "no more JACK output ports available");

	/* Activate client */
	if(jack_activate(client))
		errx(1, "cannot activate client");

	/* Connect input ports */
	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
	if(ports == NULL)
		errx(1, "no physical capture ports");

	if(jack_connect(client, ports[0], jack_port_name(input_port_front)))
		warnx("cannot connect input port front");
	if(jack_connect(client, ports[1], jack_port_name(input_port_back)))
		warnx("cannot connect input port back");

	jack_free(ports);

	/* Connect output ports */
	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if(ports == NULL)
		errx(1, "no physical playback ports");

	if(jack_connect(client, jack_port_name(output_port_left), ports[0]))
		warnx("cannot connect output port left");
	if(jack_connect(client, jack_port_name(output_port_right), ports[1]))
		warnx("cannot connect output port right");

	jack_free(ports);

	/* Signal capture */
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);

	/* Standby */
	pthread_join(jack_client_thread_id(client), NULL);

	return 0;
}
