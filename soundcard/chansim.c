/*****************************************************************************/

/*
 *      chansim.c  --  Modem simulation environment.
 *
 *      Copyright (C) 1999-2000
 *        Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soundio.h"
#include "mat.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

/* ---------------------------------------------------------------------- */


#define CHANLEN 64
#define IBUFSIZE 1024

#undef PRINTCHIN
#undef PRINTCHOUT

struct audioio_sim {
	struct audioio audioio;
	unsigned int samplerate;
	pthread_mutex_t iomutex;
	pthread_cond_t iocondrd;
	pthread_cond_t iocondwr;
	unsigned int ptr;
	unsigned int ptime;
	int16_t ibuf[IBUFSIZE];
	int16_t iostate[CHANLEN];
	float chan[CHANLEN];
	unsigned int chanlen;
	float snrmul;
	float snrstate;
	unsigned int snrmode;
        unsigned int termread;
};

/* ---------------------------------------------------------------------- */

static inline double sinc(double x)
{
        double arg = x * M_PI;

 	if (fabs(arg) < 1e-10)
                return 1;
        return sin(arg) / arg;
}

static void resamplechannel(struct audioio_sim *audioio, unsigned int chsr, unsigned int chanlen, const float *ch)
{
	double tmul, tmulch, s, en;
	unsigned int i, j;

	audioio->chanlen = CHANLEN;
	tmul = 1.0 / audioio->samplerate;
	tmulch = 1.0 / chsr;
	en = 0;
	for (i = 0; i < CHANLEN; i++) {
		s = 0;
		for (j = 0; j < chanlen; j++)
			s += ch[j] * sinc((j-0.5*(chanlen-1)) * tmulch - (i-0.5*(CHANLEN-1)) * tmul);
		audioio->chan[i] = s;
		en += s * s;
	}
	s = 0.5 / sqrt(en);
	for (i = 0; i < CHANLEN; i++)
		audioio->chan[i] *= s;
}

/* ---------------------------------------------------------------------- */

static inline int16_t calcchan(struct audioio_sim *audioio, int16_t *ptr)
{
	const float *c = audioio->chan;
	float sum, noise, ntemp;
	unsigned int i;

	sum = 0;
	for (i = 0; i < audioio->chanlen; i++, c++, ptr++)
		sum += (*ptr) * (*c);
	switch (audioio->snrmode) {
	default:
	case 0:
		noise = audioio->snrstate = randn();
		break;

	case 1:
		ntemp = audioio->snrstate;
		audioio->snrstate = randn();
		noise = ntemp - audioio->snrstate;
		break;
	}
	sum += audioio->snrmul * noise;
	if (sum > 32767)
		sum = 32767;
	if (sum < -32767)
		sum = -32767;
	return sum;
}

#define WRITECHUNK ((IBUFSIZE)/8)

static void iowrite(struct audioio *aio, const int16_t *samples, unsigned int nr)
{
	struct audioio_sim *audioio = (struct audioio_sim *)aio;
	int16_t filt[CHANLEN+WRITECHUNK];
	unsigned int i, k;

	pthread_mutex_lock(&audioio->iomutex);
	pthread_cond_broadcast(&audioio->iocondrd);
	while (nr > 0) {
		pthread_cond_wait(&audioio->iocondwr, &audioio->iomutex);
		k = nr;
		if (k > WRITECHUNK)
			k = WRITECHUNK;
		memcpy(filt, audioio->iostate, CHANLEN * sizeof(filt[0]));
		memcpy(filt+CHANLEN, samples, k * sizeof(filt[0]));
#ifdef PRINTCHIN
		for (i = 0; i < k; i++)
			fprintf(stdout, "\t%6d\n", samples[i]);
#endif
		samples += k;
		nr -= k;
		for (i = 0; i < k; i++)
			audioio->ibuf[(audioio->ptr+i) % IBUFSIZE] = calcchan(audioio, filt + i);
#ifdef PRINTCHOUT
		for (i = 0; i < k; i++)
			fprintf(stdout, "\t%6d\n", audioio->ibuf[(audioio->ptr+i) % IBUFSIZE]);
#endif
		memcpy(audioio->iostate, filt+k, CHANLEN * sizeof(filt[0]));
		audioio->ptr = (audioio->ptr + k) % IBUFSIZE;
		audioio->ptime += k;
		pthread_cond_broadcast(&audioio->iocondrd);
	}
	pthread_mutex_unlock(&audioio->iomutex);
}

static void ioread(struct audioio *aio, int16_t *samples, unsigned int nr, u_int16_t tim)
{
	struct audioio_sim *audioio = (struct audioio_sim *)aio;
	unsigned int p;
	int i;

	pthread_mutex_lock(&audioio->iomutex);
	while (nr > 0) {
                if (audioio->termread) {
                        pthread_mutex_lock(&audioio->iomutex);
                        pthread_exit(NULL);
                }
		i = (signed)(int16_t)(audioio->ptime - tim);
		if (i > IBUFSIZE) {
			pthread_mutex_unlock(&audioio->iomutex);
			i -= IBUFSIZE;
			if (i > nr)
				i = nr;
			memset(samples, 0, i * sizeof(samples[0]));
			logprintf(1, "ioread: request time %u out of time window [%u,%u)\n", tim, audioio->ptime-IBUFSIZE, audioio->ptime);
			samples += i;
			nr -= i;
			tim += i;
			pthread_mutex_lock(&audioio->iomutex);
			continue;
		}
		if (i > 0) {
			p = (IBUFSIZE + audioio->ptr - i) % IBUFSIZE;
			if (i > nr)
				i = nr;
			if (i > IBUFSIZE-p)
				i = IBUFSIZE-p;
			memcpy(samples, &audioio->ibuf[p], i * sizeof(samples[0]));
			nr -= i;
			samples += i;
			tim += i;
			continue;
		}
		pthread_cond_broadcast(&audioio->iocondwr);
		pthread_cond_wait(&audioio->iocondrd, &audioio->iomutex);
	}
	pthread_mutex_unlock(&audioio->iomutex);
}

static void ioterminateread(struct audioio *aio)
{
	struct audioio_sim *audioio = (struct audioio_sim *)aio;

	pthread_mutex_lock(&audioio->iomutex);
        audioio->termread = 1;
        pthread_mutex_unlock(&audioio->iomutex);
        pthread_cond_broadcast(&audioio->iocondwr);
}

static u_int16_t iocurtime(struct audioio *aio)
{
	struct audioio_sim *audioio = (struct audioio_sim *)aio;
        u_int16_t res;
        
	pthread_mutex_lock(&audioio->iomutex);
        res = audioio->ptime;
	pthread_mutex_unlock(&audioio->iomutex);
        return res;
}

struct modemparams ioparams_sim[] = {
	{ "simchan", "Channel Type", "Channel Type", "0 - Ideal channel", MODEMPAR_COMBO,
	  { c: { { "0 - Ideal channel", "1 - Measured channel (@11025SPS)", "2 - Measured channel (@11025SPS)" } } } },
	{ "snr", "Noise Attenuation", "Noise Attenuation (dB)", "10", MODEMPAR_NUMERIC, { n: { 0, 100, 10, 25 } } },
	{ "snrmode", "Noise Mode", "Noise Mode (Spectrum)", "0 - White Gaussian", MODEMPAR_COMBO,
	  { c: { { "0 - White Gaussian", "1 - f^2 (Limiter Discriminator)" } } } },
	{ "srate", "Sampling Rate", "Minimum Sampling Rate", "0", MODEMPAR_NUMERIC, { n: { 0, 100000, 1000, 10000 } } },
	{ NULL }
};

void iosetsnr(struct audioio *aio, float snr)
{
	struct audioio_sim *audioio = (struct audioio_sim *)aio;

	if (snr < 0)
		snr = 0;
	if (snr > 100)
		snr = 100;
       	audioio->snrmul = 16384 * pow(10, (-1.0 / 20.0) * snr);
	if (audioio->snrmode == 1)
		audioio->snrmul *= 0.5;
	logprintf(MLOG_INFO, "Simulation SNR: %gdB\n", snr);
}

static void iorelease(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;

	free(audioio);
}

void ioinit_sim(void)
{
}

struct audioio *ioopen_sim(unsigned int *samplerate, unsigned int flags, const char *params[])
{
	/* channel as measured with a Standard C558 and a C701 */
	/* sampling rate 11025 */
	static const float handychan11025[] = {
		-5.21044, -7.23703, 1.00762, 8.56528, -2.49151, -13.1699, -12.1505, -7.80964,
		-14.1223, -19.8385, -29.2846, -34.0455, -26.3771, -18.1503, -15.5361, -17.8743,
		-14.4932, 9.90765, 50.8552, 90.8269, 106.196, 75.4171, 22.7599, 1.21803,
		41.8775, 113.88, 111.28, -62.4549, -350.544, -546.694, -399.364, 173.873,
		744.975, 514.874, -207.604, -281.118, -47.7039, -6.85458, 10.1011, 13.7335,
		18.0518, 11.7933, 2.29862, 1.39829, -9.07216, -15.4798, -19.0625, -14.4499,
		0.751019, 4.3827, 0.442683, -1.23818, 1.40913, -0.139367, -2.46858, 1.62925,
		11.0248, 17.7949, 11.8747, 0.987705, -4.77601, 1.56657, -0.792208, -11.835
	};
	static const float handychan19200[] = {
			-17.1193, -15.5983, -16.0753, -17.7106, -18.1360, -13.4423, -1.1915, 18.4095,
			42.3428, 67.1847, 89.2158, 104.2136, 105.2639, 90.1995, 62.3140, 31.1091,
			7.5503, 0.9127, 16.0135, 50.0826, 93.5381, 126.0888, 122.0893, 61.0704,
			-59.4699, -221.2141, -388.5450, -514.7896, -549.2966, -447.8420, -193.3155, 179.3403,
			561.0798, 776.4116, 673.6545, 283.7702, -141.5685, -338.7276, -274.3893, -120.9636,
			-29.2744, -8.5477, -3.1922, 8.2671, 14.7713, 13.7291, 15.5945, 18.9467,
			15.2890, 6.4977, 2.3843, 2.2843, 0.8552, -5.2078, -10.9362, -14.6211,
			-16.5479, -18.8587, -18.9143, -13.2442, -3.9637, 2.9833, 4.7124, 3.0376
	};
	static const char *ctfstr[] = { "Ideal channel", "Measured channel (@11025SPS)", "Measured channel (@11025SPS)" };
	static const char *snrmodestr[] = { "White Gaussian", "f^2 (Limiter Discriminator)" };
	float snr = 100;
	unsigned int sr, ctf = 0, snrmode = 0;
	struct audioio_sim *audioio;
	
	if ((~flags) & IO_RDWR)
		return NULL;
	audioio = calloc(1, sizeof(struct audioio_sim));
	if (!audioio)
		return NULL;
	audioio->audioio.release = iorelease;
	audioio->audioio.terminateread = ioterminateread;
	audioio->audioio.transmitstart = NULL;
	audioio->audioio.transmitstop = NULL;
	audioio->audioio.write = iowrite;
	audioio->audioio.read = ioread;
	audioio->audioio.curtime = iocurtime;
	audioio->samplerate = *samplerate;
        pthread_cond_init(&audioio->iocondrd, NULL);
        pthread_cond_init(&audioio->iocondwr, NULL);
        pthread_mutex_init(&audioio->iomutex, NULL);
	if (params[0]) {
		ctf = strtoul(params[0], NULL, 0);
		if (ctf > 2)
			ctf = 2;
	}
	if (params[1])
		snr = strtod(params[1], NULL);
	if (params[2]) {
		snrmode = strtoul(params[2], NULL, 0);
		if (snrmode > 1)
			snrmode = 1;
	}
	if (params[3]) {
		sr = strtoul(params[3], NULL, 0);
		if (sr > 100000)
			sr = 100000;
		if (sr > audioio->samplerate)
			audioio->samplerate = sr;
	}
	switch(ctf) {
	default:
	case 0:
		audioio->chan[0] = 0.5;
		audioio->chanlen = 1;
		break;

	case 1:
		resamplechannel(audioio, 11025, sizeof(handychan11025)/sizeof(handychan11025[0]), handychan11025);
		break;

	case 2:
		resamplechannel(audioio, 19200, sizeof(handychan19200)/sizeof(handychan19200[0]), handychan19200);
		break;
	}		
	audioio->snrmode = snrmode;
	audioio->snrstate = 0;
	audioio->termread = 0;
	iosetsnr(&audioio->audioio, snr);
	logprintf(MLOG_INFO, "Simulation Noise: %s, Channel: %s, srate: %u\n", snrmodestr[snrmode], ctfstr[ctf], audioio->samplerate);
	*samplerate = audioio->samplerate;
        return &audioio->audioio;
}
