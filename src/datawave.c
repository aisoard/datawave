#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#include <pthread.h>
#include <jack/jack.h>
#include <complex.h>
#include <fftw3.h>

#define N 65536 // Buffer
#define G 1 // Gain

jack_client_t * client;
jack_port_t * input_port_front;
jack_port_t * input_port_back;
jack_port_t * output_port_left;
jack_port_t * output_port_right;

/* Current phase inside buffers */
int phase;

/* Input data: time and frequency domain */
float * in_time;
fftwf_complex * in_freq;
/* Input data: time to frequency domain transform */
fftwf_plan fft_in;

/* Output data: time and frequency domain */
float * out_time;
fftwf_complex * out_freq;
/* Output data: frequency to time domain transform */
fftwf_plan fft_out;

/* Impulse data: time and frequency domain */
float * impulse_time;
fftwf_complex * impulse_freq;
/* Impulse data: time to frequency transform */
fftwf_plan fft_impulse;

void init(void * arg)
{
	phase = 0;

	/* Init input buffers and transform */
	in_time = fftwf_alloc_real(N);
	in_freq = fftwf_alloc_complex(N/2+1);
	fft_in = fftwf_plan_dft_r2c_1d(N, in_time, in_freq, FFTW_MEASURE);

	/* Init output buffers and transform */
	out_time = fftwf_alloc_real(N);
	out_freq = fftwf_alloc_complex(N/2+1);
	fft_out = fftwf_plan_dft_c2r_1d(N, out_freq, out_time, FFTW_MEASURE);

	/* Init impulse buffers and transform */
	impulse_time = fftwf_alloc_real(N);
	impulse_freq = fftwf_alloc_complex(N/2+1);
	fft_impulse = fftwf_plan_dft_r2c_1d(N, impulse_time, impulse_freq, FFTW_MEASURE);

	/* Pre-compute impulse transform
	 * As this is constant the whole time, lets pre-compute it.
	 * We put the gain (G) and the renormalization (1/N) factors here to improve performance.
	 */
	memset(impulse_time, 0, sizeof(float) * N);
	impulse_time[0] = G * 1.0f / N;
	fftwf_execute(fft_impulse);
}

void fini(void * arg)
{
	/* Free input buffers and transform */
	fftwf_free(in_time);
	fftwf_free(in_freq);
	fftwf_destroy_plan(fft_in);

	/* Free output buffers and transform */
	fftwf_free(out_time);
	fftwf_free(out_freq);
	fftwf_destroy_plan(fft_out);

	/* Free impulse buffers and transform */
	fftwf_free(impulse_time);
	fftwf_free(impulse_freq);
	fftwf_destroy_plan(fft_impulse);

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

		in_time[phase_in] = (in_front[i] + in_back[i])/2;
		out_left[i] = out_time[phase_out];
		out_right[i] = out_time[phase_out];
	}

	/* Update phase */
	phase += n;
	phase %= N;

	/* Update input frequency domain */
	fftwf_execute(fft_in);

	/* Compute output frequency domain (convolution) */
	for(i = 0; i < N/2+1; i++) {
		/* Convolution */
		out_freq[i] = in_freq[i] * impulse_freq[i];
		/* Cross-correlation */
		//out_freq[i] = conjf(in_freq[i]) * impulse_freq[i];
	}

	/* Update output time domain */
	fftwf_execute(fft_out);

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
	jack_set_thread_init_callback(client, init, NULL);
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
