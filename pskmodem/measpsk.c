/*****************************************************************************/

/*
 *      measpsk.c  --  Measurement utility for the Kurtosis function.
 *
 *      Copyright (C) 1998  Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#include "meas.h"

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#ifdef HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#endif
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#include "getopt.h"

#include "mat.h"

/* ---------------------------------------------------------------------- */

static int done = 0;

/* ---------------------------------------------------------------------- */

#define SAMPLERATE   /*9600*/14400

#define LPFLEN         64
#define LPFOVER         4
#define FCARRIER     /*1800*//*3000*/2400
#define FCARRIERINC  ((((FCARRIER)<<16)+(SAMPLERATE))/(SAMPLERATE))

static const float lpfcoeff[LPFLEN] = {
              -0.000381,       -0.000855,       -0.000610,        0.000326,
               0.001286,        0.001263,       -0.000196,       -0.002150,
              -0.002614,       -0.000351,        0.003324,        0.004906,
               0.001762,       -0.004534,       -0.008329,       -0.004598,
               0.005339,        0.013047,        0.009608,       -0.005075,
              -0.019314,       -0.018044,        0.002655,        0.027887,
               0.032834,        0.004352,       -0.041718,       -0.064606,
              -0.025668,        0.079625,        0.210100,        0.299929,
               0.299929,        0.210100,        0.079625,       -0.025668,
              -0.064606,       -0.041718,        0.004352,        0.032834,
               0.027887,        0.002655,       -0.018044,       -0.019314,
              -0.005075,        0.009608,        0.013047,        0.005339,
              -0.004598,       -0.008329,       -0.004534,        0.001762,
               0.004906,        0.003324,       -0.000351,       -0.002614,
              -0.002150,       -0.000196,        0.001263,        0.001286,
               0.000326,       -0.000610,       -0.000855,       -0.000381
};

#define COSTABSHIFT  8
#define COSTABSZ     (1<<(COSTABSHIFT))

#define COS(x)       (costab[((x)>>(16-COSTABSHIFT))&(COSTABSZ-1)])
#define SIN(x)       COS((x)+0xc000)

static float costab[COSTABSZ];

/* ---------------------------------------------------------------------- */
/*
 * Linux OSS audio
 */

#if defined(HAVE_SYS_SOUNDCARD_H)

static char *soundpath = "/dev/dsp";

int sound_init(int sample_rate, int *sr)
{
        int fd, sndparam;

        fprintf(stderr, "sound: starting \"%s\"\n", soundpath);
        if ((fd = open(soundpath, O_RDWR)) < 0) {
                fprintf(stderr, "sound: Error, cannot open \"%s\"\n", soundpath);
                return -1;
        }
        sndparam = AFMT_S16_LE; /* we want 16 bits/sample signed */
        /* little endian; works only on little endian systems! */
        if (ioctl(fd, SNDCTL_DSP_SETFMT, &sndparam) == -1) {
                fprintf(stderr, "sound: Error, cannot set sample size\n");
                return -1;
        }
        if (sndparam != AFMT_S16_LE) {
                fprintf(stderr, "sound: Error, cannot set sample size to 16 bits\n");
                return -1;
        }
        sndparam = 0;   /* we want only 1 channel */
        if (ioctl(fd, SNDCTL_DSP_STEREO, &sndparam) == -1) {
                fprintf(stderr, "sound: Error, cannot set the channel number\n");
                return -1;
        }
        if (sndparam != 0) {
                fprintf(stderr, "sound: Error, cannot set the channel number to 1\n");
                return -1;
        }
        sndparam = sample_rate; 
        if(ioctl(fd, SNDCTL_DSP_SPEED, &sndparam) == -1) {
                fprintf(stderr, "sound: Error, cannot set the sample "
                        "rate\n");
                return -1;
        }
	if (sr)
		*sr = sndparam;
        return fd;
}

/* ---------------------------------------------------------------------- */
/*
 * Sun audio
 */

#elif defined(HAVE_SYS_AUDIOIO_H)

static char *soundpath = "/dev/audio";

int sound_init(int sample_rate, int *sr)
{
        audio_info_t audioinfo;
        audio_info_t audioinfo2;
	audio_device_t audiodev;
	int fd;

        fprintf(stderr, "sound: starting \"%s\"\n", soundpath);
        if ((fd = open(soundpath, O_RDWR)) < 0) {
                fprintf(stderr, "sound: Error, cannot open \"%s\"\n",  soundpath);
                return -1;
        }
        if (ioctl(fd, AUDIO_GETDEV, &audiodev) == -1) {
                fprintf(stderr, "sound: Error, cannot get audio dev\n");
                return -1;
        }       
        fprintf(stderr, "sound: Audio device: name %s, ver %s, config %s\n",
                audiodev.name, audiodev.version, audiodev.config);
        AUDIO_INITINFO(&audioinfo);
        audioinfo.play.sample_rate = audioinfo.record.sample_rate = sample_rate;
        audioinfo.play.channels = audioinfo.record.channels = 1;
        audioinfo.play.precision = audioinfo.record.precision = 16;
        audioinfo.play.encoding = audioinfo.record.encoding = AUDIO_ENCODING_LINEAR;
        //audioinfo.record.gain = 0x20;
        audioinfo.record.port = AUDIO_LINE_IN;
        //audioinfo.monitor_gain = 0;
        if (ioctl(fd, AUDIO_SETINFO, &audioinfo) == -1) {
                fprintf(stderr, "sound: Error, cannot set audio params\n");
                return -1;
        }       
        if (ioctl(fd, I_FLUSH, FLUSHR) == -1) {
                fprintf(stderr, "sound: Error, cannot flush\n");
                return -1;
        }
        if (ioctl(fd, AUDIO_GETINFO, &audioinfo2) == -1) {
                fprintf(stderr, "sound: Error, cannot set audio params\n");
                return -1;
        }       
	if (sr)
		*sr = audioinfo.record.sample_rate;
        return fd;
}

#endif

/* --------------------------------------------------------------------- */
/* 
 * Maximum length shift register connections
 * (cf. Proakis, Digital Communications, p. 399
 *
 *  2  1,2
 *  3  1,3
 *  4  1,4
 *  5  1,4
 *  6  1,6
 *  7  1,7
 *  8  1,5,6,7
 *  9  1,6
 * 10  1,8
 * 11  1,10
 * 12  1,7,9,12
 * 13  1,10,11,13
 * 14  1,5,9,14
 * 15  1,15
 * 16  1,5,14,16
 * 17  1,15
 * 18  1,12
 */

#define TAP_2 ((1<<1)|(1<<0))
#define TAP_3 ((1<<2)|(1<<0))
#define TAP_4 ((1<<3)|(1<<0))
#define TAP_5 ((1<<4)|(1<<1))
#define TAP_6 ((1<<5)|(1<<0))
#define TAP_7 ((1<<6)|(1<<0))
#define TAP_8 ((1<<7)|(1<<3)|(1<<2)|(1<<1))
#define TAP_9 ((1<<8)|(1<<3))
#define TAP_10 ((1<<9)|(1<<2))
#define TAP_11 ((1<<10)|(1<<1))
#define TAP_12 ((1<<11)|(1<<5)|(1<<3)|(1<<0))
#define TAP_13 ((1<<12)|(1<<3)|(1<<2)|(1<<0))
#define TAP_14 ((1<<13)|(1<<9)|(1<<5)|(1<<0))
#define TAP_15 ((1<<14)|(1<<0))
#define TAP_16 ((1<<15)|(1<<11)|(1<<2)|(1<<0))
#define TAP_17 ((1<<16)|(1<<2))
#define TAP_18 ((1<<17)|(1<<6))

#define MASK_2 ((1<<2)-1)
#define MASK_3 ((1<<3)-1)
#define MASK_4 ((1<<4)-1)
#define MASK_5 ((1<<5)-1)
#define MASK_6 ((1<<6)-1)
#define MASK_7 ((1<<7)-1)
#define MASK_8 ((1<<8)-1)
#define MASK_9 ((1<<9)-1)
#define MASK_10 ((1<<10)-1)
#define MASK_11 ((1<<11)-1)
#define MASK_12 ((1<<12)-1)
#define MASK_13 ((1<<13)-1)
#define MASK_14 ((1<<14)-1)
#define MASK_15 ((1<<15)-1)
#define MASK_16 ((1<<16)-1)
#define MASK_17 ((1<<17)-1)
#define MASK_18 ((1<<18)-1)

#define TAPS TAP_12
#define MASK MASK_12

/* ---------------------------------------------------------------------- */

extern __inline__ unsigned int hweight32(unsigned int w)
{
        unsigned int res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
        res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
        res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
        res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
        return (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);
}

/* ---------------------------------------------------------------------- */

#define TXBUFSZ  256

static struct txstate {
	unsigned int scrami;
	unsigned int scramq;
	unsigned int shregi;
	unsigned int shregq;
	unsigned int carphase;
	unsigned int txwr, txrd;
	int16_t txbuf[TXBUFSZ];
} txstate = { 1, 15, };

static float txcalcfilt(unsigned int bits, const float *f)
{
	unsigned int i;
	float s = 0;

	for (i = 0; i < (LPFLEN/LPFOVER); i++, f += LPFOVER, bits >>= 1)
		if (bits & 1)
			s += *f;
		else
			s -= *f;
	return s;
}

static void transmit(int fd, struct txstate *t)
{
	int ret;
	unsigned int i, j, new;
	float sr, si;

	if (t->txrd >= t->txwr) {
		t->txrd = 0;
		for (i = 0; i <= (TXBUFSZ-LPFOVER); i += LPFOVER) {
			/* update scramblers */
			new = hweight32(t->scrami & TAPS) & 1;
			t->scrami = ((t->scrami << 1) | new) & MASK;
			t->shregi = (t->shregi << 1) | new;
			new = hweight32(t->scramq & TAPS) & 1;
			t->scramq = ((t->scramq << 1) | new) & MASK;
			t->shregq = (t->shregq << 1) | new;
			/* calculate filter values */
			for (j = 0; j < LPFOVER; j++) {
				sr = txcalcfilt(t->shregi, lpfcoeff+j);
				si = txcalcfilt(t->shregq, lpfcoeff+j);
				t->txbuf[i+j] = 16000 * (sr * COS(t->carphase) - si * SIN(t->carphase));
				t->carphase += FCARRIERINC;
			}
		}
		t->txwr = i;
	}
	ret = write(fd, &t->txbuf[t->txrd], (t->txwr - t->txrd) * sizeof(t->txbuf[0]));
	if (ret < 0) {
		perror("write");
		exit(1);
	}
	t->txrd += ret / sizeof(t->txbuf[0]);
}

/* ---------------------------------------------------------------------- */

#define RXBUFSZ (4*TXBUFSZ)

static struct rxstate {
	unsigned int rxwr;
	int16_t rxbuf[RXBUFSZ];
} rxstate = { 0, };

static void receive(int fd, struct rxstate *r)
{
	int ret;

	ret = read(fd, &r->rxbuf[r->rxwr], (RXBUFSZ - r->rxwr) * sizeof(r->rxbuf[0]));
	if (ret < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		perror("read");
		exit(1);
	}
	r->rxwr = (r->rxwr + ret / sizeof(r->rxbuf[0])) % RXBUFSZ;
}

/* ---------------------------------------------------------------------- */

RETSIGTYPE sigterm()
{
	done = 1;
}

/* ---------------------------------------------------------------------- */

static int frmatprintf(const char *name, unsigned int size1, unsigned int stride1,
                       unsigned int size2, unsigned int stride2, const float *m)
{
        unsigned int i, j;
        int ret = 0;

        fprintf(stdout, "%s = [", name);
        for (i = 0; i < size1; i++) {
                for (j = 0; j < size2; j++)
                        ret += fprintf(stdout, " %g", m[i*stride1 + j*stride2]);
                if (i+1 < size1)
                        ret += fprintf(stdout, " ;\n    ");
        }
        ret += fprintf(stdout, " ];\n");
        return ret;
}

static int fcmatprintf(const char *name, unsigned int size1, unsigned int stride1,
		       unsigned int size2, unsigned int stride2, const cplxfloat_t *m)
{
	unsigned int i, j;
	int ret = 0;

	fprintf(stdout, "%s = [", name);
	for (i = 0; i < size1; i++) {
		for (j = 0; j < size2; j++) {
			ret += fprintf(stdout, " %g", real(m[i*stride1 + j*stride2]));
			if (imag(m[i*stride1 + j*stride2]) != 0)
				ret += fprintf(stdout, "%+gi", imag(m[i*stride1 + j*stride2]));
		}
		if (i+1 < size1)
			ret += fprintf(stdout, " ; ...\n    ");
	}
	ret += fprintf(stdout, " ];\n");
	return ret;
}

static cplxfloat_t crosscorr(cplxfloat_t *data, unsigned int scrami, unsigned int scramq)
{
       	unsigned int i, newi, newq;
	cplxfloat_t sum, c;

	cplx(sum, 0, 0);
	for (i = 0; i < ((RXBUFSZ-LPFLEN)/LPFOVER); i++, data += LPFOVER) {
		newi = hweight32(scrami & TAPS) & 1;
		scrami = ((scrami << 1) | newi) & MASK;
		newq = hweight32(scramq & TAPS) & 1;
		scramq = ((scramq << 1) | newq) & MASK;
		cplx(c, newi ? 1 : -1, newq ? -1 : 1);
		cmac(sum, c, *data);
	}
	return sum;
}

static inline cplxfloat_t rxfilter(const cplxfloat_t *p)
{
	cplxfloat_t r;
	const float *c = lpfcoeff;
	unsigned int i;

	cplx(r, 0, 0);
	for (i = 0; i < LPFLEN; i++, p++, c++)
		cmacs(r, *p, *c);
	return r;
}

#define CHLEN  16
#define OBSLEN 128

static void processrx(struct rxstate *rxs)
{
	cplxfloat_t rxbuf[RXBUFSZ];
	unsigned int i, j, new, pnregi, pnregq, pnregmi, pnregmq, maxidx;
	cplxfloat_t corr;
	float maxv, corrv;
	cplxfloat_t ma[OBSLEN*CHLEN], mah[CHLEN*OBSLEN], maha[CHLEN*CHLEN], mahainv[CHLEN*CHLEN], mr[OBSLEN], mahr[CHLEN], mc[CHLEN];

	/* copy it to a linear buffer and downmix it */
	new = 0;
	j = rxs->rxwr;
	for (i = 0; i < RXBUFSZ; i++) {
		cplx(rxbuf[i], rxs->rxbuf[j] * COS(new), rxs->rxbuf[j] * SIN(new));
		j++;
		if (j >= RXBUFSZ) 
			j = 0;
		new -= FCARRIERINC;
	}
	/* run lowpass filter over the receiver buffer (inplace) */
	for (i = 0; i <= (RXBUFSZ-LPFLEN); i++)
		rxbuf[i] = rxfilter(&rxbuf[i]);
	/* print received vector */
	printf("rxvec = [\n");
	for (i = 0; i < RXBUFSZ; i++)
		printf("  %g%+gi\n", real(rxbuf[i]), imag(rxbuf[i]));
	printf("];\n\n");
	/* calculate and print code correlation */
	printf("codecorr = [\n");
	maxv = 0;
	maxidx = 0;
	pnregmi = pnregi = 1;
	pnregmq = pnregq = 15;  /* must match transmitter! */
	do {
		for (i = 0; i < LPFOVER; i++) {
			corr = crosscorr(rxbuf+i, pnregi, pnregq);
			corrv = real(corr) * real(corr) + imag(corr) * imag(corr);
			if (fabs(corrv) > fabs(maxv)) {
				maxv = corrv;
				maxidx = i;
				pnregmi = pnregi;
				pnregmq = pnregq;
			}
			printf("  %g%+gi\n", real(corr), imag(corr));
		}
		new = hweight32(pnregi & TAPS) & 1;
		pnregi = ((pnregi << 1) | new) & MASK;
		new = hweight32(pnregq & TAPS) & 1;
		pnregq = ((pnregq << 1) | new) & MASK;
	} while (pnregi != 1);
	printf("];\n\n");
	if (maxv < 0)
		printf("%% channel seems to be inverted\n\n");
	/* build and print A matrix */
	for (i = 0; i < OBSLEN; i++) {
		pnregi = pnregmi;
		pnregq = pnregmq;
		for (j = 0; j < CHLEN; j++) {
			new = hweight32(pnregi & TAPS) & 1;
			pnregi = ((pnregi << 1) | new) & MASK;
			real(ma[i*CHLEN+j]) = new ? 1 : -1;
			new = hweight32(pnregq & TAPS) & 1;
			pnregq = ((pnregq << 1) | new) & MASK;
			imag(ma[i*CHLEN+j]) = new ? 1 : -1;
		}
		new = hweight32(pnregmi & TAPS) & 1;
		pnregmi = ((pnregmi << 1) | new) & MASK;
		new = hweight32(pnregmq & TAPS) & 1;
		pnregmq = ((pnregmq << 1) | new) & MASK;
	}
	fcmatprintf("a", OBSLEN, CHLEN, CHLEN, 1, ma);
	/* transpose it */
	fchermtranspose(mah, ma, OBSLEN, CHLEN);
	fcmatprintf("ah", CHLEN, OBSLEN, OBSLEN, 1, mah);
	fcmul(maha, mah, ma, CHLEN, OBSLEN, CHLEN);
	fcmatprintf("aha", CHLEN, CHLEN, CHLEN, 1, maha);
	fcinv(mahainv, maha, CHLEN);
	fcmatprintf("ahainv", CHLEN, CHLEN, CHLEN, 1, mahainv);
	/* build received vector */
	for (i = 0; i < OBSLEN; i++)
		mr[i] = rxbuf[CHLEN/2*LPFOVER+i*LPFOVER+maxidx];
	fcmatprintf("r", 1, 0, OBSLEN, 1, mr);
	fcmul(mahr, mah, mr, CHLEN, OBSLEN, 1);
	fcmatprintf("ahr", 1, 0, CHLEN, 1, mahr);
	fcmul(mc, mahainv, mahr, CHLEN, CHLEN, 1);
	fcmatprintf("mc", 1, 0, CHLEN, 1, mc);
	printf("%%\nmcf = fft(mc) / sqrt(sum(abs(mc).^2));\nsemilogy((0:size(mcf,2)-1)/size(mcf,2)*2400,abs(mcf));\n");
}

/* ---------------------------------------------------------------------- */

#define OUTBUFSIZE 1024
#define INBUFSIZE 65536

int main(int argc, char *argv[])
{
        static const struct option long_options[] = {
                {0, 0, 0, 0}
        };
	struct pollfd pfd[1];
	int fd, i, c;

	while ((c = getopt_long (argc, argv, "", long_options, NULL)) != EOF) {
		switch (c) {
		default:
			fprintf(stderr, "usage: meas \n");
			exit(1);
		}
	}
	/* initialize cosine table */
	for (i = 0; i < COSTABSZ; i++)
		costab[i] = cos(i * (2.0 * M_PI / COSTABSZ));
	/* start sound IO */
	if ((fd = sound_init(SAMPLERATE, &c)) == -1) {
		fprintf(stderr, "Cannot open sound interface\n");
		exit(1);
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	printf("%% Sampling rate requested %d, actual %d\n", SAMPLERATE, c);
	signal(SIGTERM, sigterm);
	signal(SIGHUP, sigterm);
	signal(SIGINT, sigterm);
	signal(SIGQUIT, sigterm);
	do {
		pfd[0].fd = fd;
		pfd[0].events = POLLIN | POLLOUT;
		i = poll(pfd, 1, 1000);
		if (i < 0) {
			if (errno == EINTR)
				break;
			perror("poll");
			exit(1);
		}
		if (!i || !(pfd[0].revents & (POLLIN | POLLOUT))) {
			fprintf(stderr, "poll timeout\n");
			exit(1);
		}
		if (pfd[0].revents & POLLIN)
			receive(fd, &rxstate);
		if (pfd[0].revents & POLLOUT)
			transmit(fd, &txstate);
	} while (!done);
	processrx(&rxstate);
	close(fd);
	exit(0);
}
