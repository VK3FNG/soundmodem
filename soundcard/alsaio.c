/*****************************************************************************/

/*
 *      alsaio.c  --  Audio I/O using the ALSA API.
 *
 *      Copyright (C) 1999-2000, 2004
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soundio.h"
#include "audioio.h"

#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#ifdef HAVE_ALSA

#include <alsa/asoundlib.h>

/* ---------------------------------------------------------------------- */

#define AUDIOIBUFSIZE 4096

struct audioio_unix {
	struct audioio audioio;
	unsigned int samplerate;
	unsigned int capturechannelmode;
	snd_pcm_t *playback_handle;
	snd_pcm_t *capture_handle;
	unsigned int fragsize;
	pthread_mutex_t iomutex;
	pthread_cond_t iocond;
	unsigned int flags;
	unsigned int ptr;
	u_int16_t ptime;
	int16_t ibuf[AUDIOIBUFSIZE];
};

struct modemparams ioparams_alsasoundcard[] = {
	{ "device", "ALSA Audio Driver", "Path name of the audio (soundcard) driver", "hw:0,0", MODEMPAR_COMBO, 
	  { c: { { "hw:0,0", "plughw:0,0", "hw:1,0", "plughw:1,0", "hw:2,0", "plughw:2,0", "hw:3,0", "plughw:3,0" } } } },
        { "halfdup", "Half Duplex", "Force operating the Sound Driver in Half Duplex mode", "0", MODEMPAR_CHECKBUTTON },
	{ "capturechannelmode", "Capture Channel", "Capture Channel", "Mono", MODEMPAR_COMBO, 
	  { c: { { "Mono", "Left", "Right" } } } },
	{ NULL, }
};

#define CAP_HALFDUPLEX   0x100

#define FLG_READING      0x1000
#define FLG_HALFDUPLEXTX 0x2000
#define FLG_TERMINATERX  0x4000

#ifndef INFTIM
#define INFTIM (-1)
#endif

/* ---------------------------------------------------------------------- */

static void iorelease(struct audioio *aio);
static void iowrite(struct audioio *aio, const int16_t *samples, unsigned int nr);
static void ioread(struct audioio *aio, int16_t *samples, unsigned int nr, u_int16_t tim);
static u_int16_t iocurtime(struct audioio *aio);
static void iotransmitstart(struct audioio *aio);
static void iotransmitstop(struct audioio *aio);
static void ioterminateread(struct audioio *aio);

/* ---------------------------------------------------------------------- */

static inline int iomodetofmode(unsigned int flags)
{
	switch (flags & IO_RDWR) {
	default:
	case IO_RDONLY:
		return O_RDONLY;

	case IO_WRONLY:
		return O_WRONLY;

	case IO_RDWR:
		return O_RDWR;
	}
}

/* ---------------------------------------------------------------------- */

static snd_pcm_t *open_alsa(const char *name, snd_pcm_stream_t direction, unsigned int *samplerate, unsigned int *chanmode)
{
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_format_mask_t *fmtmask;
	snd_pcm_sw_params_t *swparams;
	/* set fragment size so we have approx. 10-20ms wakeup latency */
	unsigned int buffer_time = 500000;              /* ring buffer length in us */
	unsigned int period_time = 15000;               /* period time in us */
	snd_pcm_format_t samplefmt;
	snd_pcm_uframes_t buffer_size;
	snd_pcm_uframes_t period_size;
        int err, dir;

	if (snd_pcm_open(&pcm_handle, name, direction, 0) < 0) {
		logprintf(MLOG_ERROR, "alsa: Error opening PCM device %s\n", name);
		return NULL;
	}
	/* 
	 * Set hardware parameter
	 */
	snd_pcm_hw_params_alloca(&hwparams);
        /* choose all parameters */
        err = snd_pcm_hw_params_any(pcm_handle, hwparams);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Broken configuration for capture: no configurations available: %s\n", snd_strerror(err));
                goto err;
        }
        /* set the interleaved read/write format */
        err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Access type RW_INTERLEAVED not available for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* set the sample format */
	snd_pcm_format_mask_alloca(&fmtmask);
	snd_pcm_format_mask_set(fmtmask, SND_PCM_FORMAT_S16);
        err = snd_pcm_hw_params_set_format_mask(pcm_handle, hwparams, fmtmask);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Sample format S16_NE not available for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* set the count of channels */
	if (chanmode && *chanmode) {
		err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2);
		if (err < 0) {
			logprintf(MLOG_ERROR, "Channels count (2) not available for captures: %s; trying Mono\n", snd_strerror(err));
			*chanmode = 0;
		}
	}
	if (!chanmode || !*chanmode) {
		err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1);
		if (err < 0) {
			logprintf(MLOG_ERROR, "Channels count (1) not available for captures: %s\n", snd_strerror(err));
			goto err;
		}
	}
        /* set the stream rate */
        err = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, samplerate, 0);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Rate %iHz not available for capture: %s\n", *samplerate, snd_strerror(err));
                goto err;
        }
        /* set the buffer time */
        err = snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &buffer_time, &dir);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set buffer time %i for capture: %s\n", buffer_time, snd_strerror(err));
                goto err;
        }
        /* set the period time */
        err = snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &period_time, &dir);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set period time %i for capture: %s\n", period_time, snd_strerror(err));
                goto err;
        }
        /* write the parameters to device */
        err = snd_pcm_hw_params(pcm_handle, hwparams);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set hw params for capture: %s\n", snd_strerror(err));
                goto err;
        }
	/* read current configuration */
        err = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to get buffer size for capture: %s\n", snd_strerror(err));
                goto err;
        }
        err = snd_pcm_hw_params_get_period_size(hwparams, &period_size, &dir);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to get period size for capture: %s\n", snd_strerror(err));
                goto err;
        }
	err = snd_pcm_hw_params_get_rate(hwparams, samplerate, &dir);
	if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to get the sample rate for capture: %s\n", snd_strerror(err));
                goto err;
        }
	err = snd_pcm_hw_params_get_format(hwparams, &samplefmt);
	if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to get the sample format for capture: %s\n", snd_strerror(err));
                goto err;
        }
	err = snd_pcm_hw_params_get_sbits(hwparams);
	if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to get the number of significant bits for capture: %s\n", snd_strerror(err));
                goto err;
        }
	printf("ALSA: Using sample rate %u, sample format %d, significant bits %d, buffer size %u, period size %u\n",
	       *samplerate, (int)samplefmt, err, (unsigned int)buffer_size, (unsigned int)period_size);
	/*
	 * Set Software Parameters
	 */
	snd_pcm_sw_params_alloca(&swparams);
        /* get the current swparams */
        err = snd_pcm_sw_params_current(pcm_handle, swparams);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to determine current swparams for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* start the transfer when samples are available */
        err = snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, 64);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set start threshold mode for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* allow the transfer when at least 1 samples can be processed */
        err = snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, 1);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set avail min for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* align all transfers to 1 sample */
        err = snd_pcm_sw_params_set_xfer_align(pcm_handle, swparams, 1);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set transfer align for capture: %s\n", snd_strerror(err));
                goto err;
        }
        /* write the parameters to the capture device */
        err = snd_pcm_sw_params(pcm_handle, swparams);
        if (err < 0) {
                logprintf(MLOG_ERROR, "Unable to set sw params for capture: %s\n", snd_strerror(err));
                goto err;
        }
	if (snd_pcm_prepare(pcm_handle) < 0) {
		logprintf(MLOG_ERROR, "Error preparing capture.\n");
		goto err;
	}
	return pcm_handle;

  err:
	snd_pcm_close(pcm_handle);
	return NULL;
}

/* ---------------------------------------------------------------------- */
/*
 * Linux ALSA audio
 */

struct audioio *ioopen_alsasoundcard(unsigned int *samplerate, unsigned int flags, const char *params[])
{
	const char *audiopath = params[0];
	struct audioio_unix *audioio;
	unsigned int prate, crate, i;

	audioio = calloc(1, sizeof(struct audioio_unix));
	if (!audioio)
		return NULL;
	audioio->audioio.release = iorelease;
	if (!audiopath)
		audiopath = "hw:0,0";
	prate = crate = *samplerate;
	audioio->capturechannelmode = 0;
	if (params[2] && !strcmp(params[2], ioparams_alsasoundcard[2].u.c.combostr[1]))
		audioio->capturechannelmode = 1;
	else if (params[2] && !strcmp(params[2], ioparams_alsasoundcard[2].u.c.combostr[2]))
		audioio->capturechannelmode = 2;
	/* todo: remove configurations with different rx/tx rates or make different rates work...*/
	if (flags & IO_RDONLY) {
		audioio->audioio.terminateread = ioterminateread;
		audioio->audioio.read = ioread;
		audioio->audioio.curtime = iocurtime;
		audioio->capture_handle = open_alsa(audiopath, SND_PCM_STREAM_CAPTURE, &crate, &audioio->capturechannelmode);
		if (!audioio->capture_handle)
			goto err;
	}
	if (flags & IO_WRONLY) {
		audioio->audioio.transmitstart = iotransmitstart;
		audioio->audioio.transmitstop = iotransmitstop;
		audioio->audioio.write = iowrite;
		i = 0;
		audioio->playback_handle = open_alsa(audiopath, SND_PCM_STREAM_PLAYBACK, &prate, &i);
		if (!audioio->playback_handle)
			goto err;
	}
	audioio->samplerate = prate;
	if ((flags & (IO_RDONLY|IO_WRONLY)) == (IO_RDONLY|IO_WRONLY) && abs(prate - crate) > 1) {
                logprintf(MLOG_ERROR, "audio: Error, playback/capture sample rates do not match: %u/%u\n", prate, crate);
		goto err;
	}		
        pthread_cond_init(&audioio->iocond, NULL);
        pthread_mutex_init(&audioio->iomutex, NULL);
        audioio->flags = flags & IO_RDWR;
	audioio->ptr = audioio->ptime = 0;
        logprintf(MLOG_DEBUG, "audio: starting \"%s\"\n", audiopath);
	if (params[1] && params[1][0] != '0') {
		audioio->flags |= CAP_HALFDUPLEX;
		logprintf(MLOG_INFO, "audio: forcing half duplex mode\n");
	}
	*samplerate = audioio->samplerate;
        return &audioio->audioio;

  err:
	if (audioio->playback_handle)
		snd_pcm_close(audioio->playback_handle);
	if (audioio->capture_handle)
		snd_pcm_close(audioio->capture_handle);
	free(audioio);
	return NULL;
}

static inline void iotxend(struct audioio_unix *audioio)
{
	int err;

	err = snd_pcm_drain(audioio->playback_handle);
	if (err < 0)
		logprintf(MLOG_ERROR, "snd_pcm_drain in iotxend: %s", snd_strerror(err));
	if (!(audioio->flags & CAP_HALFDUPLEX))
		return;
	err = snd_pcm_start(audioio->capture_handle);
	if (err < 0 && err != -EBADFD)
		logprintf(MLOG_ERROR, "snd_pcm_start in iotxend: %s", snd_strerror(err));
}

static inline void iotxstart(struct audioio_unix *audioio)
{
	int err;

	if (snd_pcm_prepare(audioio->playback_handle) < 0) {
                logprintf(MLOG_ERROR, "Error preparing tx.\n");
        }
	err = snd_pcm_start(audioio->playback_handle);
	if (err < 0)
		logprintf(MLOG_ERROR, "snd_pcm_start in iotxstart: %s", snd_strerror(err));
}

/* ---------------------------------------------------------------------- */

static void iorelease(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;

	pthread_cond_destroy(&audioio->iocond);
	pthread_mutex_destroy(&audioio->iomutex);
        audioio->flags = audioio->ptr = audioio->ptime = 0;
	if (audioio->playback_handle)
		snd_pcm_close(audioio->playback_handle);
	if (audioio->capture_handle)
		snd_pcm_close(audioio->capture_handle);
	free(audioio);
}

static void iowrite(struct audioio *aio, const int16_t *samples, unsigned int nr)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;
	unsigned char *p = (unsigned char *)samples;
	int err;
        
	if (!audioio->playback_handle)
		return;
	err = snd_pcm_writei(audioio->playback_handle, p, nr);
	if (err == -EPIPE) {
		if (snd_pcm_prepare(audioio->playback_handle) < 0) {
			logprintf(MLOG_ERROR, "Error preparing tx.\n");
		}
		err = snd_pcm_writei(audioio->playback_handle, p, nr);
	}
	if (err < 0) {
		logprintf(MLOG_ERROR, "audio: snd_pcm_writei: %s\n", snd_strerror(err));
		return;
	}
	if (err < nr) {
		logprintf(MLOG_ERROR, "audio: snd_pcm_writei: not enough samples written: %d < %u\n", err, nr);
		return;
	}
}

static void ioread(struct audioio *aio, int16_t *samples, unsigned int nr, u_int16_t tim)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;
	int16_t ibuf[2*AUDIOIBUFSIZE/8];
	int16_t *ip;
	unsigned int p;
	int i, j;

	pthread_mutex_lock(&audioio->iomutex);
	while (nr > 0) {
                if (audioio->flags & FLG_TERMINATERX) {
                        pthread_mutex_unlock(&audioio->iomutex);
                        pthread_exit(NULL);
                }
		i = (signed)(int16_t)(audioio->ptime - tim);
		if (i > AUDIOIBUFSIZE) {
			pthread_mutex_unlock(&audioio->iomutex);
			i -= AUDIOIBUFSIZE;
			if (i > nr)
				i = nr;
			memset(samples, 0, i * sizeof(samples[0]));
			logprintf(MLOG_ERROR, "ioread: request time %u out of time window [%u,%u)\n", tim, audioio->ptime-AUDIOIBUFSIZE, audioio->ptime);
			samples += i;
			nr -= i;
			tim += i;
			pthread_mutex_lock(&audioio->iomutex);
			continue;
		}
		if (i > 0) {
			p = (AUDIOIBUFSIZE + audioio->ptr - i) % AUDIOIBUFSIZE;
			if (i > nr)
				i = nr;
			if (i > AUDIOIBUFSIZE-p)
				i = AUDIOIBUFSIZE-p;
			memcpy(samples, &audioio->ibuf[p], i * sizeof(samples[0]));
			nr -= i;
			samples += i;
			tim += i;
			continue;
		}
		if (audioio->flags & (FLG_READING|FLG_HALFDUPLEXTX)) {
			pthread_cond_wait(&audioio->iocond, &audioio->iomutex);
			continue;
		}
		audioio->flags |= FLG_READING;
		pthread_mutex_unlock(&audioio->iomutex);
		if (!audioio->capture_handle)
			logerr(MLOG_FATAL, "audio: read: capture handle NULL");
		i = snd_pcm_readi(audioio->capture_handle, ibuf, sizeof(ibuf)/sizeof(ibuf[0])/2);
		if (i == -EPIPE) {
			if (snd_pcm_prepare(audioio->capture_handle) < 0) {
				logprintf(MLOG_ERROR, "Error preparing rx.\n");
			}
			i = snd_pcm_readi(audioio->capture_handle, ibuf, sizeof(ibuf)/sizeof(ibuf[0])/2);
		}
		if (i < 0)
			logprintf(MLOG_FATAL, "audio: snd_pcm_readi: %s", snd_strerror(i));
		if (!i) {
			logerr(MLOG_ERROR, "audio: snd_pcm_readi returned 0??");
			pthread_mutex_lock(&audioio->iomutex);
			audioio->flags &= ~FLG_READING;
			pthread_cond_broadcast(&audioio->iocond);
			continue;
		}
		p = i;
		pthread_mutex_lock(&audioio->iomutex);
		audioio->flags &= ~FLG_READING;
		ip = ibuf;
		if (audioio->capturechannelmode)
			ip += audioio->capturechannelmode-1;
		for (; p > 0; ) {
			i = p;
			if (i > AUDIOIBUFSIZE-audioio->ptr)
				i = AUDIOIBUFSIZE-audioio->ptr;
			if (audioio->capturechannelmode) {
				for (j = 0; j < i; j++, ip += 2)
					audioio->ibuf[audioio->ptr + j] = *ip;
			} else {
				memcpy(&audioio->ibuf[audioio->ptr], ip, i * sizeof(audioio->ibuf[0]));
				ip += i;
			}
			audioio->ptr = (audioio->ptr + i) % AUDIOIBUFSIZE;
			audioio->ptime += i;
			p -= i;
		}
		pthread_cond_broadcast(&audioio->iocond);
	}
	pthread_mutex_unlock(&audioio->iomutex);
}

static u_int16_t iocurtime(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;
        u_int16_t res;
	int16_t ibuf[2*AUDIOIBUFSIZE/8];
	int16_t *ip;
	unsigned int p;
	int i, j, r;

	pthread_mutex_lock(&audioio->iomutex);
	for (;;) {
		if (audioio->flags & (FLG_READING|FLG_HALFDUPLEXTX))
			break;
		audioio->flags |= FLG_READING;
		pthread_mutex_unlock(&audioio->iomutex);
		if (!audioio->capture_handle)
			logprintf(MLOG_FATAL, "audio: read: capture handle NULL");
		r = snd_pcm_nonblock(audioio->capture_handle, 1);
		if (r < 0)
			logprintf(MLOG_FATAL, "audio: snd_pcm_nonblock: %s", snd_strerror(r));
		i = snd_pcm_readi(audioio->capture_handle, ibuf, sizeof(ibuf)/sizeof(ibuf[0])/2);
		r = snd_pcm_nonblock(audioio->capture_handle, 0);
		if (r < 0)
			logprintf(MLOG_FATAL, "audio: snd_pcm_nonblock: %s", snd_strerror(r));
		if (!i || i == -EAGAIN) {
			pthread_mutex_lock(&audioio->iomutex);
			audioio->flags &= ~FLG_READING;
                        pthread_cond_broadcast(&audioio->iocond);
			break;
		}
		if (i < 0)
			logprintf(MLOG_FATAL, "audio: snd_pcm_readi: %s", snd_strerror(i));
		p = i;
		pthread_mutex_lock(&audioio->iomutex);
		audioio->flags &= ~FLG_READING;
		ip = ibuf;
		if (audioio->capturechannelmode)
			ip += audioio->capturechannelmode-1;
		for (; p > 0; ) {
			i = p;
			if (i > AUDIOIBUFSIZE-audioio->ptr)
				i = AUDIOIBUFSIZE-audioio->ptr;
			if (audioio->capturechannelmode) {
				for (j = 0; j < i; j++, ip += 2)
					audioio->ibuf[audioio->ptr + j] = *ip;
			} else {
				memcpy(&audioio->ibuf[audioio->ptr], ip, i * sizeof(audioio->ibuf[0]));
				ip += i;
			}
			audioio->ptr = (audioio->ptr + i) % AUDIOIBUFSIZE;
			audioio->ptime += i;
			p -= i;
		}
		pthread_cond_broadcast(&audioio->iocond);
	}
        res = audioio->ptime;
	pthread_mutex_unlock(&audioio->iomutex);
        return res;
}

static void iotransmitstart(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;
	if (audioio->flags & CAP_HALFDUPLEX) {
		pthread_mutex_lock(&audioio->iomutex);
		audioio->flags |= FLG_HALFDUPLEXTX;
		while (audioio->flags & FLG_READING)
			pthread_cond_wait(&audioio->iocond, &audioio->iomutex);
		pthread_mutex_unlock(&audioio->iomutex);
	}
	iotxstart(audioio);
}

static void iotransmitstop(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;

#if 0
	short sbuf[256];
	unsigned int i, j;
	/* add 20ms tail */
	i = audioio->samplerate / 50;
	memset(sbuf, 0, sizeof(sbuf));
	while (i > 0) {
		j = sizeof(sbuf)/sizeof(sbuf[0]);
		if (j > i)
			j = i;
		iowrite(audioio, sbuf, j);
		i -= j;
	}
#endif
        iotxend(audioio);
	if (audioio->flags & CAP_HALFDUPLEX) {
		pthread_mutex_lock(&audioio->iomutex);
		audioio->flags &= ~FLG_HALFDUPLEXTX;
		pthread_cond_broadcast(&audioio->iocond);
		pthread_mutex_unlock(&audioio->iomutex);
	}
}

static void ioterminateread(struct audioio *aio)
{
	struct audioio_unix *audioio = (struct audioio_unix *)aio;

	pthread_mutex_lock(&audioio->iomutex);
        audioio->flags |= FLG_TERMINATERX;
        pthread_mutex_unlock(&audioio->iomutex);
	pthread_cond_broadcast(&audioio->iocond);
}

/* ---------------------------------------------------------------------- */
#else

struct audioio *ioopen_alsasoundcard(unsigned int *samplerate, unsigned int flags, const char *params[])
{
	return NULL;
}

#endif

void ioinit_alsasoundcard(void)
{
}

