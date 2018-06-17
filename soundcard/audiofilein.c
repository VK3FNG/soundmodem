/*****************************************************************************/

/*
 *      audiofilein.c  --  Audio input from file.
 *
 *      Copyright (C) 2000
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

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include "soundio.h"
#include "audioio.h"

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <audiofile.h>

/* ---------------------------------------------------------------------- */

#define AUDIOIBUFSIZE 4096

struct audioio_filein {
	struct audioio audioio;
	AFfilehandle file;
	/* audio file info */
	unsigned int samplerate;
	int samplefmt, samplewidth, byteorder;
	unsigned int nrchan, framesz;
	AFframecount nrframes, frameptr;
	pthread_mutex_t iomutex;
	pthread_cond_t iocond;
	unsigned int flags;
	unsigned int ptr;
	u_int16_t ptime;
	int16_t ibuf[AUDIOIBUFSIZE];
};

struct modemparams ioparams_filein[] = {
	{ "file", "Input file", "Input File Name", "", MODEMPAR_STRING },
        { "repeat", "Repeat", "Loop input file forever", "0", MODEMPAR_CHECKBUTTON },
	{ NULL, }
};

#define FLG_TERMINATERX  4
#define FLG_REPEAT       8

/* ---------------------------------------------------------------------- */

static void iorelease(struct audioio *aio)
{
	struct audioio_filein *audioio = (struct audioio_filein *)aio;

	if (audioio->file) {
		afCloseFile(audioio->file);
                pthread_cond_destroy(&audioio->iocond);
                pthread_mutex_destroy(&audioio->iomutex);
        }
	audioio->file = NULL;
        audioio->flags = audioio->ptr = audioio->ptime = 0;
	free(audioio);
}

static void ioread(struct audioio *aio, int16_t *samples, unsigned int nr, u_int16_t tim)
{
	struct audioio_filein *audioio = (struct audioio_filein *)aio;
	unsigned int p, nrsamp;
	unsigned char *buf, *bp;
	int i;

	buf = alloca(AUDIOIBUFSIZE/8 * audioio->framesz);
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
		pthread_mutex_unlock(&audioio->iomutex);
		nrsamp = AUDIOIBUFSIZE/8;
		if (audioio->flags & FLG_REPEAT && audioio->frameptr >= audioio->nrframes) {
			audioio->frameptr = 0;
			afSeekFrame(audioio->file, AF_DEFAULT_TRACK, 0);
		}
		if (audioio->frameptr >= audioio->nrframes) {
			if (audioio->frameptr >= audioio->nrframes + 2 * AUDIOIBUFSIZE)
				pthread_exit(NULL);
			pthread_mutex_lock(&audioio->iomutex);
			for (p = 0; p < nrsamp; p++) {
				audioio->ibuf[audioio->ptr] = 0;
				audioio->ptr = (audioio->ptr + 1) % AUDIOIBUFSIZE;
				audioio->ptime += 1;
				audioio->frameptr++;
			}
		} else {
			if (audioio->frameptr + nrsamp > audioio->nrframes)
				nrsamp = audioio->nrframes - audioio->frameptr;
			afReadFrames(audioio->file, AF_DEFAULT_TRACK, buf, nrsamp);
			audioio->frameptr += nrsamp;
			pthread_mutex_lock(&audioio->iomutex);
			for (bp = buf, p = 0; p < nrsamp; p++, bp += audioio->framesz) {
				switch (audioio->samplefmt) {
				case AF_SAMPFMT_TWOSCOMP:
				case AF_SAMPFMT_UNSIGNED:
					if (audioio->byteorder == AF_BYTEORDER_BIGENDIAN) {
						switch (audioio->samplewidth) {
						case 8:
							i = ((signed char *)bp)[0] << 8;
							break;
						
						case 16:
						case 24:
						case 32:
							i = (((signed char *)bp)[0] << 8) | bp[1];
							break;
						}
					} else {
						switch (audioio->samplewidth) {
						case 8:
							i = ((signed char *)bp)[0] << 8;
							break;

						case 16:
							i = (((signed char *)bp)[1] << 8) | bp[0];
							break;

						case 24:
							i = (((signed char *)bp)[2] << 8) | bp[1];
							break;
						case 32:
							i = (((signed char *)bp)[3] << 8) | bp[2];
							break;
						}
					}
					if (audioio->samplefmt == AF_SAMPFMT_UNSIGNED)
						i ^= 0x8000;
					break;

				default:
					i = 0;
				}
				audioio->ibuf[audioio->ptr] = i;
				audioio->ptr = (audioio->ptr + 1) % AUDIOIBUFSIZE;
				audioio->ptime += 1;
			}
		}
		pthread_cond_broadcast(&audioio->iocond);
	}
	pthread_mutex_unlock(&audioio->iomutex);
}

static u_int16_t iocurtime(struct audioio *aio)
{
	struct audioio_filein *audioio = (struct audioio_filein *)aio;
        u_int16_t res;

	pthread_mutex_lock(&audioio->iomutex);
        res = audioio->ptime;
	pthread_mutex_unlock(&audioio->iomutex);
        return res;
}

static void ioterminateread(struct audioio *aio)
{
	struct audioio_filein *audioio = (struct audioio_filein *)aio;

	pthread_mutex_lock(&audioio->iomutex);
        audioio->flags |= FLG_TERMINATERX;
        pthread_mutex_unlock(&audioio->iomutex);
	pthread_cond_broadcast(&audioio->iocond);
}

/* ---------------------------------------------------------------------- */

struct audioio *ioopen_filein(unsigned int *samplerate, unsigned int flags, const char *params[])
{
	const char *audiopath = params[0];
	struct audioio_filein *audioio;
	int version;

	if ((flags & IO_RDWR) != IO_RDONLY) {
		logprintf(MLOG_ERROR, "audio: File input is unidirectional\n");
		return NULL;
	}
	audioio = calloc(1, sizeof(struct audioio_filein));
	if (!audioio)
		return NULL;
	audioio->audioio.release = iorelease;
	audioio->audioio.terminateread = ioterminateread;
	audioio->audioio.transmitstart = NULL;
	audioio->audioio.transmitstop = NULL;
	audioio->audioio.write = NULL;
	audioio->audioio.read = ioread;
	audioio->audioio.curtime = iocurtime;
        pthread_cond_init(&audioio->iocond, NULL);
        pthread_mutex_init(&audioio->iomutex, NULL);
        audioio->flags = audioio->ptr = audioio->ptime = 0;
	if (params[1] && params[1][0] != '0')
		audioio->flags |= FLG_REPEAT;
	if (!audiopath) {
		logprintf(MLOG_ERROR, "audio: No file name specified\n");
		free(audioio);
		return NULL;
	}
	if (!(audioio->file = afOpenFile(audiopath, "r", NULL))) {
		logprintf(MLOG_ERROR, "audio: Cannot open file \"%s\"\n", audiopath);
		free(audioio);
                return NULL;
        }
	audioio->samplerate = afGetRate(audioio->file, AF_DEFAULT_TRACK);
	afGetSampleFormat(audioio->file, AF_DEFAULT_TRACK, &audioio->samplefmt, &audioio->samplewidth);
	audioio->byteorder = afGetByteOrder(audioio->file, AF_DEFAULT_TRACK);
	audioio->nrchan = afGetChannels(audioio->file, AF_DEFAULT_TRACK);
	audioio->framesz = afGetFrameSize(audioio->file, AF_DEFAULT_TRACK, 1);
	audioio->nrframes = afGetFrameCount(audioio->file, AF_DEFAULT_TRACK);
	audioio->frameptr = 0;
	/* work around a libaudiofile WAVE bug */
	if (afGetFileFormat(audioio->file, &version) == AF_FILE_WAVE && audioio->samplewidth <= 8)
		audioio->samplefmt = AF_SAMPFMT_UNSIGNED;
	logprintf(MLOG_INFO, "audio: sample rate %u channels %u sfmt %d swidth %d byteorder %d framesize %u nrframes %d\n",
		  audioio->samplerate, audioio->nrchan, audioio->samplefmt, audioio->samplewidth,
		  audioio->byteorder, audioio->framesz, audioio->nrframes);
	*samplerate = audioio->samplerate;
        return &audioio->audioio;
}

/* ---------------------------------------------------------------------- */

void ioinit_filein(void)
{
}

/* ---------------------------------------------------------------------- */
