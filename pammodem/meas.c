/*****************************************************************************/

/*
 *      meas.c  --  Measurement utility for the PAM channel.
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

#include "getopt.h"

#include "mat.h"

/* ---------------------------------------------------------------------- */

//#define SAMPLERATE 9600
#define SAMPLERATE 11025
//#define SAMPLERATE 19200

/* ---------------------------------------------------------------------- */

static int done = 0;

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

#define TAPS TAP_15
#define MASK MASK_15

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
	unsigned int scram;
	unsigned int txwr, txrd;
	int16_t txbuf[TXBUFSZ];
} txstate = { 1, };

static void transmit(int fd, struct txstate *t)
{
	int ret;
	unsigned int i, new;

	if (t->txrd >= t->txwr) {
		t->txrd = 0;
		t->txwr = TXBUFSZ;
		for (i = 0; i < TXBUFSZ; i++) {
			new = hweight32(t->scram & TAPS) & 1;
			t->scram = ((t->scram << 1) | new) & MASK;
			t->txbuf[i] = new ? 32000 : -32000;
		}
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

static int crosscorr(int16_t *data, unsigned int scram)
{
       	unsigned int i, new, sum = 0;

	for (i = 0; i < RXBUFSZ; i++, data++) {
		new = hweight32(scram & TAPS) & 1;
		scram = ((scram << 1) | new) & MASK;
		if (new)
			sum += *data;
		else
			sum -= *data;
	}
	return sum;
}

#define CHLEN  64
#define OBSLEN 512

static void processrx(struct rxstate *rxs)
{
	int16_t rxbuf[RXBUFSZ];
	unsigned int i, j, new, pnreg, pnreg2;
	int maxv, corrv;
	float ma[OBSLEN*CHLEN], mat[CHLEN*OBSLEN], mata[CHLEN*CHLEN], matainv[CHLEN*CHLEN], mr[OBSLEN], matr[CHLEN], mc[CHLEN];

	memcpy(rxbuf, &rxs->rxbuf[rxs->rxwr], (RXBUFSZ-rxs->rxwr) * sizeof(rxbuf[0]));
	memcpy(rxbuf + (RXBUFSZ-rxs->rxwr), rxs->rxbuf, rxs->rxwr * sizeof(rxbuf[0]));
	/* print received vector */
	printf("rxvec = [\n");
	for (i = 0; i < RXBUFSZ; i++)
		printf("  %6d\n", (int)rxbuf[i]);
	printf("];\n\n");
	/* calculate and print code correlation */
	printf("codecorr = [\n");
	maxv = 0;
	pnreg2 = pnreg = 1;
	do {
		corrv = crosscorr(rxbuf, pnreg);
		if (abs(corrv) > abs(maxv)) {
			maxv = corrv;
			pnreg2 = pnreg;
		}
		printf("  %d\n", corrv);
		new = hweight32(pnreg & TAPS) & 1;
		pnreg = ((pnreg << 1) | new) & MASK;
	} while (pnreg != 1);
	printf("];\n\n");
	if (maxv < 0)
		printf("%% channel seems to be inverted\n\n");
	/* build and print A matrix */
	for (i = 0; i < OBSLEN; i++) {
		pnreg = pnreg2;
		for (j = 0; j < CHLEN; j++) {
			new = hweight32(pnreg & TAPS) & 1;
			pnreg = ((pnreg << 1) | new) & MASK;
			ma[i*CHLEN+j] = new ? 1 : -1;
		}
		new = hweight32(pnreg2 & TAPS) & 1;
		pnreg2 = ((pnreg2 << 1) | new) & MASK;
	}
	frmatprintf("a", OBSLEN, CHLEN, CHLEN, 1, ma);
	/* transpose it */
	frtranspose(mat, ma, OBSLEN, CHLEN);
	frmatprintf("at", CHLEN, OBSLEN, OBSLEN, 1, mat);
	frmul(mata, mat, ma, CHLEN, OBSLEN, CHLEN);
	frmatprintf("ata", CHLEN, CHLEN, CHLEN, 1, mata);
	frinv(matainv, mata, CHLEN);
	frmatprintf("atainv", CHLEN, CHLEN, CHLEN, 1, matainv);
	/* build received vector */
	for (i = 0; i < OBSLEN; i++)
		mr[i] = rxbuf[CHLEN/2+i];
	frmatprintf("r", 1, 0, OBSLEN, 1, mr);
	frmul(matr, mat, mr, CHLEN, OBSLEN, 1);
	frmatprintf("atr", 1, 0, CHLEN, 1, matr);
	frmul(mc, matainv, matr, CHLEN, CHLEN, 1);
	frmatprintf("mc", 1, 0, CHLEN, 1, mc);
	printf("%%\nmcf = fft(mc) / sqrt(sum(mc .* mc));\nsemilogy((0:size(mcf,2)-1)/size(mcf,2)*9600,abs(mcf));\n");
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
