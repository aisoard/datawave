#include "common.h"

/* Sinus function of x with period of l */
float sine(float l, float x)
{
	return sin(TAU * x/l);
}

/* Gaussian function of x with characteristic length of a */
float gauss(float a, float x)
{
	return expf(- (x*x) / (a*a));
}

/* Amplify near -1 and 1 values, reduce near 0 values */
float filter(float s, float x)
{
	if(x*x < s*s)
		return 0.0f;
	else
		if(x > 0.0f)
			return 1.0f;
		else
			return -1.0f;
}

/* Noise procedure with amplitude between min and max */
float noise(float min, float max)
{
	return min + (float) rand() / ((float) RAND_MAX / (max - min));
}

/* Convolution of kernel against data */
void convolution(float * input_time, fftwf_complex * kernel_freq, float * output_time)
{
	memcpy(wave_time, input_time, sizeof(float) * N);

	fftwf_execute(fft_wave_time_to_freq);

	for(int i = 0; i < N/2+1; i++)
		wave_freq[i] = wave_freq[i] * kernel_freq[i] / N;

	fftwf_execute(fft_wave_freq_to_time);

	memcpy(output_time, wave_time, sizeof(float) * N);
}

/* Correlation of kernel against data */
void correlation(float * input_time, fftwf_complex * kernel_freq, float * output_time)
{
	memcpy(wave_time, input_time, sizeof(float) * N);

	fftwf_execute(fft_wave_time_to_freq);

	for(int i = 0; i < N/2+1; i++)
		wave_freq[i] = wave_freq[i] * conjf(kernel_freq[i]) / N;

	fftwf_execute(fft_wave_freq_to_time);

	memcpy(output_time, wave_time, sizeof(float) * N);
}

/* Autocorrelation of data */
void autocorrelation(float * data_time, float * data_auto)
{
	memcpy(wave_time, data_time, sizeof(float) * N);

	fftwf_execute(fft_wave_time_to_freq);

	for(int i = 0; i < N/2+1; i++) {
		fftwf_complex coef = wave_freq[i];
		wave_freq[i] = coef * conjf(coef) / N;
	}

	fftwf_execute(fft_wave_freq_to_time);

	memcpy(data_auto, wave_time, sizeof(float) * N);
}

/* Generate a random pulse */
void pulse(float * data_time)
{
	for(int i = 0; i < N; i++) {
		float x = i < N/2 ? i : i-N;
		data_time[i] = noise(-1.0f, 1.0f) * gauss(64.0f, x);
	}
}

/* Amplify functor */
void amplify(float * data_time, float a)
{
	for(int i = 0; i < N; i++)
		data_time[i] *= a;
}

/* Normalize data */
float normalize(float * data_time)
{
	float max = 0.0f;

	for(int i = 0; i < N; i++) {
		float data = data_time[i];
		max = MAX(max, data * data);
	}

	max = sqrt(max);

	amplify(data_time, 1.0f/max);

	return max;
}
