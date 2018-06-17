#ifndef _MISC_H
#define _MISC_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------------- */

/*
 * Hamming weight (number of bits that are ones).
 */
extern inline unsigned int hweight32(unsigned int w) 
{
	w = (w & 0x55555555) + ((w >>  1) & 0x55555555);
	w = (w & 0x33333333) + ((w >>  2) & 0x33333333);
	w = (w & 0x0F0F0F0F) + ((w >>  4) & 0x0F0F0F0F);
	w = (w & 0x00FF00FF) + ((w >>  8) & 0x00FF00FF);
	w = (w & 0x0000FFFF) + ((w >> 16) & 0x0000FFFF);
	return w;
}

extern inline unsigned int hweight16(unsigned short w)
{
	w = (w & 0x5555) + ((w >> 1) & 0x5555);
	w = (w & 0x3333) + ((w >> 2) & 0x3333);
	w = (w & 0x0F0F) + ((w >> 4) & 0x0F0F);
	w = (w & 0x00FF) + ((w >> 8) & 0x00FF);
	return w;
}

extern inline unsigned int hweight8(unsigned char w)
{
	w = (w & 0x55) + ((w >> 1) & 0x55);
	w = (w & 0x33) + ((w >> 2) & 0x33);
	w = (w & 0x0F) + ((w >> 4) & 0x0F);
	return w;
}

/* ---------------------------------------------------------------------- */

/*
 * Reverse order of bits.
 */
extern inline unsigned int rbits32(unsigned int w)
{
	w = ((w >>  1) & 0x55555555) | ((w <<  1) & 0xaaaaaaaa);
	w = ((w >>  2) & 0x33333333) | ((w <<  2) & 0xcccccccc);
	w = ((w >>  4) & 0x0f0f0f0f) | ((w <<  4) & 0xf0f0f0f0);
	w = ((w >>  8) & 0x00ff00ff) | ((w <<  8) & 0xff00ff00);
	w = ((w >> 16) & 0x0000ffff) | ((w << 16) & 0xffff0000);
	return w;
}

extern inline unsigned short rbits16(unsigned short w)
{
	w = ((w >> 1) & 0x5555) | ((w << 1) & 0xaaaa);
	w = ((w >> 2) & 0x3333) | ((w << 2) & 0xcccc);
	w = ((w >> 4) & 0x0f0f) | ((w << 4) & 0xf0f0);
	w = ((w >> 8) & 0x00ff) | ((w << 8) & 0xff00);
	return w;
}

extern inline unsigned char rbits8(unsigned char w)
{
	w = ((w >> 1) & 0x55) | ((w << 1) & 0xaa);
	w = ((w >> 2) & 0x33) | ((w << 2) & 0xcc);
	w = ((w >> 4) & 0x0f) | ((w << 4) & 0xf0);
	return w;
}

/* ---------------------------------------------------------------------- */

extern inline float avg(float average, float input, int scale)
{
	int i;

	input -= average;
	for (i = 0; i < scale; i++)
		input /= 2;

	return (average + input);
}

extern inline float avg2(float average, float input, float weight)
{
	return input * weight + average * (1.0 - weight);
}

extern inline float phaseavg(float *data, int len)
{
	float sum = 0.0;
	float min = M_PI;
	float max = -M_PI;
	int i;

	for (i = 0; i < len; i++) {
		sum += data[i];
		min = min < data[i] ? min : data[i];
		max = max > data[i] ? max : data[i];
	}
	return (sum - min - max) / (len - 2);
}

/* ---------------------------------------------------------------------- */

#endif				/* _MISC_H */
