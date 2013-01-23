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

	/* Import system wisdom */
	if(!fftwf_import_system_wisdom())
		warnx("no fftwf wisdom found, please run:\n"
		      "# fftwf-wisdom -o /etc/fftw/wisdomf rif%d rib%d rof%d rob%d", N, N, N, N);

	/* Init input sound buffers and transform */
	in_sound_time = fftwf_alloc_real(N);
	in_sound_freq = fftwf_alloc_complex(N/2+1);
	fft_in_sound = fftwf_plan_dft_r2c_1d(N, in_sound_time, in_sound_freq, FFTW_WISDOM_ONLY | FFTW_PATIENT);

	/* Init input data buffers and transform */
	in_data_freq = fftwf_alloc_complex(N/2+1);
	in_data_time = fftwf_alloc_real(N);
	fft_in_data = fftwf_plan_dft_c2r_1d(N, in_data_freq, in_data_time, FFTW_WISDOM_ONLY | FFTW_PATIENT);

	/* Init output data buffers and transform */
	out_data_time = fftwf_alloc_real(N);
	out_data_freq = fftwf_alloc_complex(N/2+1);
	fft_out_data = fftwf_plan_dft_r2c_1d(N, out_data_time, out_data_freq, FFTW_WISDOM_ONLY | FFTW_PATIENT);

	/* Init output sound buffers and transform */
	out_sound_freq = fftwf_alloc_complex(N/2+1);
	out_sound_time = fftwf_alloc_real(N);
	fft_out_sound = fftwf_plan_dft_c2r_1d(N, out_sound_freq, out_sound_time, FFTW_WISDOM_ONLY | FFTW_PATIENT);

	/* Init impulse buffers and transform */
	impulse_time = fftwf_alloc_real(N);
	impulse_freq = fftwf_alloc_complex(N/2+1);
	fft_impulse_t2f = fftwf_plan_dft_r2c_1d(N, impulse_time, impulse_freq, FFTW_WISDOM_ONLY | FFTW_PATIENT);
	fft_impulse_f2t = fftwf_plan_dft_c2r_1d(N, impulse_freq, impulse_time, FFTW_WISDOM_ONLY | FFTW_PATIENT);

	/* Pre-compute impulse and outpulse frequency domain */
	pulse(impulse_time);
	fftwf_execute(fft_impulse_t2f);
	for(int i = 0; i < N/2+1; i++) {
		fftwf_complex impulse = impulse_freq[i];
		impulse_freq[i] = impulse/cabsf(impulse);
	}
}

void fini(void * arg)
{
	/* Free input sound buffers and transform */
	fftwf_destroy_plan(fft_in_sound);
	fftwf_free(in_sound_time);
	fftwf_free(in_sound_freq);

	/* Free input data buffers and transform */
	fftwf_destroy_plan(fft_in_data);
	fftwf_free(in_data_freq);
	fftwf_free(in_data_time);

	/* Free output data buffers and transform */
	fftwf_destroy_plan(fft_out_data);
	fftwf_free(out_data_time);
	fftwf_free(out_data_freq);

	/* Free output sound buffers and transform */
	fftwf_destroy_plan(fft_out_sound);
	fftwf_free(out_sound_freq);
	fftwf_free(out_sound_time);

	/* Free impulse buffers and transform */
	fftwf_destroy_plan(fft_impulse_t2f);
	fftwf_destroy_plan(fft_impulse_f2t);
	fftwf_free(impulse_time);
	fftwf_free(impulse_freq);

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
		int phase_out = (phase + i + N/2 + (-n)/2) % N;

		in_sound_time[phase_in] = (in_left[i] + in_right[i]) / 2;
		out_left[i] = out_sound_time[phase_out];
		out_right[i] = out_sound_time[phase_out];
	}

	/* Update phase */
	phase += n;
	phase %= N;

	/* Update input sound frequency domain */
	fftwf_execute(fft_in_sound);

	/* Compute input data frequency domain (cross-correlation with anti-impulse) */
	for(i = 0; i < N/2+1; i++)
		in_data_freq[i] = conjf(in_sound_freq[i]) * impulse_freq[i] * G_IN/N; /* FIXME Auto-correlation normalization */

	/* Update input data time domain */
	fftwf_execute(fft_in_data);

	/* Analyse in data and generate out data */
	analyse(in_data_time, out_data_time);

	/* Update output data frequency domain */
	fftwf_execute(fft_out_data);

	/* Compute output sound frequency domain (convolution) */
	for(i = 0; i < N/2+1; i++)
		out_sound_freq[i] = out_data_freq[i] * impulse_freq[i] * G_OUT/N;

	/* Update output sound time domain */
	fftwf_execute(fft_out_sound);

	return 0;
}
