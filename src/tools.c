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

/* Noise procedure with amplitude between min and max */
float noise(float min, float max)
{
	return min + (float) rand() / ((float) RAND_MAX / (max - min));
}

/* Generate a random pulse */
void pulse(float * data_time)
{
	for(int i = 0; i < N; i++) {
		float x = i < N/2 ? i : i-N;
		data_time[i] = noise(-1.0f, 1.0f) * gauss(64.0f, x);
	}
}

/* Amplify functor of a coefficient a */
void amplify(float * data_time, float a)
{
	for(int i = 0; i < N; i++)
		data_time[i] *= a;
}

/* Normalizing functor */
void normalize(float * data_time)
{
	float max2 = 0;

	for(int i = 0; i < N; i++) {
		float temp = data_time[i];
		float temp2 = temp * temp;
		max2 = max2 > temp2 ? max2 : temp2;
	}

	float factor = 1 / sqrt(max2);

	amplify(data_time, factor);
}
