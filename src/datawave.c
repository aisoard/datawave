#include "common.h"

/* Read input data and write output data */
void analyse(float * in_data_time, float * out_data_time)
{
	for(int i = 0; i < N; i++)
		out_data_time[i] = filter(0.9375f, 16.0f, in_data_time[i]);

	out_data_time[0] = 1.0f;
}

void init(void * arg)
{
	phase = 0;
	FILE * wisdom = NULL;

	/* Import wisdom */
	wisdom = fopen("fftwf_wisdom", "r");

	if(wisdom == NULL) {
		warn("Cannot open `fftwf_wisdom' in read mode");
		warnx("No wisdom found. Wisdom will be created and saved for next time, be patient...");
	} else {
		fftwf_import_wisdom_from_file(wisdom);
		fclose(wisdom);
	}

	/* Init wave buffers and transforms */
	wave = fftwf_malloc(MAX(sizeof(float) * N, sizeof(fftwf_complex) * (N/2+1)));
	wave_time = (float *) wave;
	wave_freq = (fftwf_complex *) wave;
	fft_wave_time_to_freq = fftwf_plan_dft_r2c_1d(N, wave_time, wave_freq, FFTW_PATIENT | FFTW_DESTROY_INPUT);
	fft_wave_freq_to_time = fftwf_plan_dft_c2r_1d(N, wave_freq, wave_time, FFTW_PATIENT | FFTW_DESTROY_INPUT);

	/* Init input and output buffers */
	in_sound_time = fftwf_alloc_real(N);
	in_data_time = fftwf_alloc_real(N);
	out_data_time = fftwf_alloc_real(N);
	out_sound_time = fftwf_alloc_real(N);

	/* Init impulse buffers and transform */
	impulse_time = fftwf_alloc_real(N);
	impulse_freq = fftwf_alloc_complex(N/2+1);
	impulse_auto = fftwf_alloc_real(N);
	fft_impulse_time_to_freq = fftwf_plan_dft_r2c_1d(N, impulse_time, impulse_freq, FFTW_PATIENT | FFTW_DESTROY_INPUT);
	fft_impulse_freq_to_time = fftwf_plan_dft_c2r_1d(N, impulse_freq, impulse_time, FFTW_PATIENT | FFTW_DESTROY_INPUT);

	/* Export wisdom */
	wisdom = fopen("fftwf_wisdom", "w");

	if(wisdom == NULL) {
		warn("Cannot open or create `fftwf_wisdom' in write mode");
		warnx("No wisdom saved");
	} else {
		fftwf_export_wisdom_to_file(wisdom);
		fclose(wisdom);
	}

	/* Pre-compute impulse time, frequency and autocorrelation domain */
	srand(time(NULL));
	pulse(impulse_time);
	fftwf_execute(fft_impulse_time_to_freq);
	autocorrelation(impulse_time, impulse_auto);
}

void fini(void * arg)
{
	/* Free input and output buffers */
	fftwf_free(in_sound_time);
	fftwf_free(in_data_time);
	fftwf_free(out_data_time);
	fftwf_free(out_sound_time);

	/* Free impulse buffers and transform */
	fftwf_destroy_plan(fft_impulse_time_to_freq);
	fftwf_destroy_plan(fft_impulse_freq_to_time);
	fftwf_free(impulse_time);
	fftwf_free(impulse_freq);
	fftwf_free(impulse_auto);

	/* Free wave buffers and transforms */
	fftwf_destroy_plan(fft_wave_time_to_freq);
	fftwf_destroy_plan(fft_wave_freq_to_time);
	fftwf_free(wave);

	exit(0);
}

int exec(jack_nframes_t n, void * arg)
{
	int i;

	/* Connect both input and both output */
	jack_default_audio_sample_t * in_left = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port_left, n);
	jack_default_audio_sample_t * in_right = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port_right, n);
	jack_default_audio_sample_t * out_left = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_left, n);
	jack_default_audio_sample_t * out_right = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_right, n);

	/* Copy the input data at the current phase and send output data at the opposed phase
	 * Suppose N multiple of n
	 */
	for(i = 0; i < n; i++) {
		int phase_in = phase + i;
		int phase_out = (phase + i + N/2) % N;

		in_sound_time[phase_in] = (in_left[i] + in_right[i]) / 2;
		out_left[i] = out_sound_time[phase_out];
		out_right[i] = out_sound_time[phase_out];
	}

	/* Update phase */
	phase += n;
	phase %= N;

	/* Generate input data from input sound by correlation against impulse */
	correlation(in_sound_time, impulse_freq, in_data_time);

	/* Amplify input data and correct autocorrelation */
	amplify(in_data_time, G_IN / impulse_auto[0]);

	/* Analyse input data and generate output data */
	analyse(in_data_time, out_data_time);

	/* Amplify output data */
	amplify(out_data_time, G_OUT);

	/* Generate output sound from output data by convolution against impulse */
	convolution(out_data_time, impulse_freq, out_sound_time);

	return 0;
}
