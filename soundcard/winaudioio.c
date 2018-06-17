/*****************************************************************************/

/*
 *      winaudioio.c  --  Audio I/O for Win32 (DirectX).
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

#include "audioio.h"

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include <directx.h>

/* ---------------------------------------------------------------------- */

struct audioio_directx {
	struct audioio audioio;
	unsigned int samplerate;
        LPDIRECTSOUND dsplay;
        LPDIRECTSOUNDCAPTURE dsrec;
        LPDIRECTSOUNDBUFFER playbuf;
        LPDIRECTSOUNDCAPTUREBUFFER recbuf;
        DWORD playbufsz, playptr, recbufsz;
        unsigned int playstart;
	pthread_mutex_t iomutex;
	unsigned int flags;
	unsigned int ptr;
	u_int16_t ptime;
};

static struct dsdrivers {
        GUID guid;
        char name[64];
} dsplaydrv[8], dscaptdrv[8];

static HANDLE hinst;

struct modemparams ioparams_soundcard[] = {
	{ "dsplay", "Output Driver", "Name of the output driver", dsplaydrv[0].name, MODEMPAR_COMBO, 
	  { c: { { dsplaydrv[0].name, dsplaydrv[1].name, dsplaydrv[2].name, dsplaydrv[3].name,
                   dsplaydrv[4].name, dsplaydrv[5].name, dsplaydrv[6].name, dsplaydrv[7].name } } } },
	{ "dscapture", "Input Driver", "Name of the output driver", dscaptdrv[0].name, MODEMPAR_COMBO, 
	  { c: { { dscaptdrv[0].name, dscaptdrv[1].name, dscaptdrv[2].name, dscaptdrv[3].name,
                   dscaptdrv[4].name, dscaptdrv[5].name, dscaptdrv[6].name, dscaptdrv[7].name } } } },
	{ NULL, }
};

#define FLG_ARMINCREMENT  0x100
#define FLG_TERMINATERX   0x200

/* ---------------------------------------------------------------------- */

static BOOL CALLBACK DSEnumProc(LPGUID guid, LPCSTR lpszDesc, LPCSTR lpszDrvName, LPVOID lpcontext)
{
        struct dsdrivers *drv = lpcontext;
        unsigned int i;

        for (i = 0; i < 8 && drv->name[0]; i++, drv++);
        if (i >= 8)
                return TRUE;
        if (guid)
                drv->guid = *guid;
        else
                drv->guid = GUID_NULL;
        if (!lpszDrvName)
                snprintf(drv->name, sizeof(drv->name), "%s", lpszDesc);
        else
                snprintf(drv->name, sizeof(drv->name), "%s (%s)", lpszDesc, lpszDrvName);
        drv->name[sizeof(drv->name)-1] = 0;
        logprintf(MLOG_INFO, "has %sGUID, desc %s drvname %s\n", guid ? "" : "no ", lpszDesc, lpszDrvName);
        return TRUE;
}

void ioinit_soundcard(void)
{
        unsigned int i;
        
        memset(dsplaydrv, 0, sizeof(dsplaydrv));
        memset(dscaptdrv, 0, sizeof(dscaptdrv));
        logprintf(MLOG_INFO, "DirectSound drivers\n");
        DirectSoundEnumerateA(DSEnumProc, dsplaydrv);
        logprintf(MLOG_INFO, "DirectSoundCapture drivers\n");
        DirectSoundCaptureEnumerateA(DSEnumProc, dscaptdrv);
        for (i = 0; i < 8 && dsplaydrv[i].name[0]; i++);
        for (; i < 8; i++)
                ioparams_soundcard[0].u.c.combostr[i] = NULL;
        for (i = 0; i < 8 && dscaptdrv[i].name[0]; i++);
        for (; i < 8; i++)
                ioparams_soundcard[1].u.c.combostr[i] = NULL;
        hinst = GetModuleHandleA(0);
}

/* ---------------------------------------------------------------------- */

static void iorelease(struct audioio *aio)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;

	if (audioio->flags & IO_WRONLY) {
		IDirectSoundBuffer_Stop(audioio->playbuf);
		IDirectSoundBuffer_Release(audioio->playbuf);
	}
	if (audioio->flags & IO_RDONLY) {
		IDirectSoundCaptureBuffer_Stop(audioio->recbuf);
		IDirectSoundCaptureBuffer_Release(audioio->recbuf);
		IDirectSoundCapture_Release(audioio->dsrec);
	}
	if (audioio->flags & IO_WRONLY)
		IDirectSound_Release(audioio->dsplay);
	free(audioio);
}

static void iowrite(struct audioio *aio, const int16_t *samples, unsigned int nr)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;
        HRESULT res;
        DWORD cappos, playszhalf, playszquart;
        void *lptr1, *lptr2;
        DWORD lbytes1, lbytes2;
        unsigned int free;

        playszhalf = audioio->playbufsz/2;
	playszquart = playszhalf / 2;
	while (nr > 0) {
                if (FAILED(res = IDirectSoundBuffer_GetCurrentPosition(audioio->playbuf, &cappos, NULL)))
                        logprintf(MLOG_FATAL, "IDirectSoundBuffer_GetCurrentPosition error 0x%lx\n", res);
                cappos /= 2;
		/* compute buffer fill grade */
                free = (playszhalf + audioio->playptr - cappos) % playszhalf;
		if (free >= playszquart) {
			/* underrun */
			audioio->playptr = cappos;
			free = 0;
		}
		free = playszquart - 1 - free;
		if (!free) {
			Sleep(20);
			continue;
		}
		if (free > nr)
			free = nr;
                if (FAILED(res = IDirectSoundBuffer_Lock(audioio->playbuf, audioio->playptr*2, free*2, &lptr1, &lbytes1, &lptr2, &lbytes2, 0)))
                        logprintf(MLOG_FATAL, "IDirectSoundBuffer_Lock error 0x%lx\n", res);
		memcpy(lptr1, samples, lbytes1);
		if (lbytes2)
			memcpy(lptr2, ((char *)samples)+lbytes1, lbytes2);
		if (FAILED(res = IDirectSoundBuffer_Unlock(audioio->playbuf, lptr1, lbytes1, lptr2, lbytes2)))
			logprintf(MLOG_FATAL, "IDirectSoundBuffer_Unlock error 0x%lx\n", res);
		samples += free;
		nr -= free;
		audioio->playptr = (audioio->playptr + free) % playszhalf;
	}
}

/* use the "safe to read" pointer; the hwpointer gives bad data */

static void ioread(struct audioio *aio, int16_t *samples, unsigned int nr, u_int16_t tim)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;
        HRESULT res;
        DWORD cappos, recszhalf, cpos2;
        u_int16_t ctime;
	int i;
        void *lptr1, *lptr2;
        DWORD lbytes1, lbytes2;
        unsigned int p;
        
        recszhalf = audioio->recbufsz/2;
	pthread_mutex_lock(&audioio->iomutex);
	while (nr > 0) {
                if (audioio->flags & FLG_TERMINATERX) {
                        pthread_mutex_unlock(&audioio->iomutex);
                        pthread_exit(NULL);
                }                        
                if (FAILED(res = IDirectSoundCaptureBuffer_GetCurrentPosition(audioio->recbuf, &cpos2, &cappos)))
                        logprintf(MLOG_FATAL, "IDirectSoundCaptureBuffer_GetCurrentPosition error 0x%lx\n", res);
                //logprintf(MLOG_INFO, "Cappositions: %6u %6u\n", cappos, cpos2);
		if (cappos < recszhalf) {
                        if (audioio->flags & FLG_ARMINCREMENT) {
                                audioio->flags &= ~FLG_ARMINCREMENT;
                                audioio->ptime += recszhalf;
                        }
                } else {
                        audioio->flags |= FLG_ARMINCREMENT;
                }
                cappos /= 2;
                ctime = audioio->ptime + cappos;
		i = (signed)(int16_t)(ctime - tim);
		if (i > recszhalf) {
			pthread_mutex_unlock(&audioio->iomutex);
			i -= recszhalf;
			if (i > nr)
				i = nr;
			memset(samples, 0, i * sizeof(samples[0]));
			logprintf(MLOG_ERROR, "ioread: request time %u out of time window [%u,%u)\n", tim, ctime-recszhalf, ctime);
			samples += i;
			nr -= i;
			tim += i;
			pthread_mutex_lock(&audioio->iomutex);
			continue;
		}
		if (i > 0) {
			p = (recszhalf + cappos - i) % recszhalf;
			if (i > nr)
				i = nr;
                        if (FAILED(res = IDirectSoundCaptureBuffer_Lock(audioio->recbuf, 2*p, 2*i, &lptr1, &lbytes1, &lptr2, &lbytes2, 0)))
                                logprintf(MLOG_FATAL, "IDirectSoundCaptureBuffer_Lock error 0x%lx\n", res);
                        memcpy(samples, lptr1, lbytes1);
                        if (lbytes2)
                                memcpy(((char *)samples)+lbytes1, lptr2, lbytes2);
                        if (FAILED(res = IDirectSoundCaptureBuffer_Unlock(audioio->recbuf, lptr1, lbytes1, lptr2, lbytes2)))
                                logprintf(MLOG_FATAL, "IDirectSoundCaptureBuffer_Unlock error 0x%lx\n", res);
                        nr -= i;
			samples += i;
			tim += i;
			continue;
		}
                pthread_mutex_unlock(&audioio->iomutex);
                Sleep(50);
                pthread_mutex_lock(&audioio->iomutex);
	}
	pthread_mutex_unlock(&audioio->iomutex);
}

static u_int16_t iocurtime(struct audioio *aio)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;
        HRESULT res;
        DWORD cappos, recszhalf;
        u_int16_t ctime;
        
        recszhalf = audioio->recbufsz/2;
	pthread_mutex_lock(&audioio->iomutex);
        if (FAILED(res = IDirectSoundCaptureBuffer_GetCurrentPosition(audioio->recbuf, NULL, &cappos)))
                logprintf(MLOG_FATAL, "IDirectSoundCaptureBuffer_GetCurrentPosition error 0x%lx\n", res);
        if (cappos < recszhalf) {
                if (audioio->flags & FLG_ARMINCREMENT) {
                        audioio->flags &= ~FLG_ARMINCREMENT;
                        audioio->ptime += recszhalf;
                }
        } else {
                audioio->flags |= FLG_ARMINCREMENT;
        }
        ctime = audioio->ptime + cappos/2;
	pthread_mutex_unlock(&audioio->iomutex);
        return ctime;
}

static void iotransmitstart(struct audioio *aio)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;
	HRESULT res;
	DWORD cappos;

        /* needed for half duplex */
	if (FAILED(res = IDirectSoundBuffer_GetCurrentPosition(audioio->playbuf, &cappos, NULL)))
		logprintf(MLOG_FATAL, "IDirectSoundBuffer_GetCurrentPosition error 0x%lx\n", res);
	audioio->playptr = cappos / 2;
}

static void iotransmitstop(struct audioio *aio)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;
        HRESULT res;
        void *lptr1;
        DWORD lbytes1;
        unsigned int i, j;
	int16_t zbuf[128];

	/* fill half of the buffer with zero */
	memset(zbuf, 0, sizeof(zbuf));
	i = audioio->playbufsz/4;
	while (i > 0) {
		j = i;
		if (j > sizeof(zbuf)/sizeof(zbuf[0]))
			j = sizeof(zbuf)/sizeof(zbuf[0]);
		iowrite(&audioio->audioio, zbuf, j);
		i -= j;
	}
	/* playback finished, zero whole buffer */
	if (FAILED(res = IDirectSoundBuffer_Lock(audioio->playbuf, 0, audioio->playbufsz, &lptr1, &lbytes1, NULL, NULL, 0)))
                logprintf(MLOG_FATAL, "DirectSoundBufferLock error 0x%lx\n", res);
 	memset(lptr1, 0, lbytes1);
	if (FAILED(res = IDirectSoundBuffer_Unlock(audioio->playbuf, lptr1, lbytes1, NULL, 0)))
		logprintf(MLOG_FATAL, "DirectSoundBufferUnlock error 0x%lx\n", res);
}

static void ioterminateread(struct audioio *aio)
{
	struct audioio_directx *audioio = (struct audioio_directx *)aio;

	pthread_mutex_lock(&audioio->iomutex);
        audioio->flags |= FLG_TERMINATERX;
        pthread_mutex_unlock(&audioio->iomutex);
}

struct audioio *ioopen_soundcard(unsigned int *samplerate, unsigned int flags, const char *params[])
{
        LPGUID lpguid;
        HRESULT res;
        WAVEFORMATEX waveformat;
        DSBUFFERDESC bdesc;
        DSCBUFFERDESC cbdesc;
        DSCAPS caps;
        DSCCAPS ccaps;
        DSBCAPS bcaps;
        DSCBCAPS cbcaps;
        unsigned int i, isprimary = 0;
        void *lptr1;
        DWORD lbytes1;
        HWND hwnd = GetDesktopWindow();
	struct audioio_directx *audioio;

	audioio = calloc(1, sizeof(struct audioio_directx));
	if (!audioio)
		return NULL;
	audioio->audioio.release = iorelease;
	if (flags & IO_RDONLY) {
		audioio->audioio.terminateread = ioterminateread;
		audioio->audioio.read = ioread;
		audioio->audioio.curtime = iocurtime;
	}
	if (flags & IO_WRONLY) {
		audioio->audioio.transmitstart = iotransmitstart;
		audioio->audioio.transmitstop = iotransmitstop;
		audioio->audioio.write = iowrite;
	}
	audioio->samplerate = *samplerate;
        pthread_mutex_init(&audioio->iomutex, NULL);
        audioio->playptr = audioio->ptr = audioio->ptime = audioio->playstart = 0;
	audioio->flags = flags & IO_RDWR;
	if (audioio->flags & IO_WRONLY) {
		lpguid = NULL;
		if (params[0])
			for (i = 0; i < 8; i++)
				if (dsplaydrv[i].name && !strcmp(dsplaydrv[i].name, params[0]) &&
				    memcmp(&dsplaydrv[i].guid, &GUID_NULL, sizeof(dsplaydrv[i].guid)))
					lpguid = &dsplaydrv[i].guid;
		if (FAILED(res = DirectSoundCreate(lpguid, &audioio->dsplay, NULL))) {
			logprintf(MLOG_ERROR, "DirectSoundCreate error 0x%lx\n", res);
			goto errdscreate;
		}
		if (FAILED(res = IDirectSound_SetCooperativeLevel(audioio->dsplay, hwnd, DSSCL_WRITEPRIMARY))) {
			logprintf(MLOG_WARNING, "SetCooperativeLevel DSSCL_WRITEPRIMARY error 0x%lx\n", res);
			if (FAILED(res = IDirectSound_SetCooperativeLevel(audioio->dsplay, hwnd, DSSCL_EXCLUSIVE))) {
				logprintf(MLOG_ERROR, "SetCooperativeLevel DSSCL_EXCLUSIVE error 0x%lx\n", res);
				goto errdsb;
			}
		} else
			isprimary = 1;
	}
	if (audioio->flags & IO_RDONLY) {
		lpguid = NULL;
		if (params[1])
			for (i = 0; i < 8; i++)
				if (dscaptdrv[i].name && !strcmp(dscaptdrv[i].name, params[1]) &&
				    memcmp(&dscaptdrv[i].guid, &GUID_NULL, sizeof(dscaptdrv[i].guid)))
					lpguid = &dscaptdrv[i].guid;
		if (FAILED(res = DirectSoundCaptureCreate(lpguid, &audioio->dsrec, NULL))) {
			logprintf(MLOG_ERROR, "DirectSoundCaptureCreate error 0x%lx\n", res);
			goto errdsb;
		}
	}
        /* DirectSound capabilities */
 	if (audioio->flags & IO_WRONLY) {
		caps.dwSize = sizeof(caps);
		if (FAILED(res = IDirectSound_GetCaps(audioio->dsplay, &caps))) {
			logprintf(MLOG_ERROR, "DirectSoundGetCaps error 0x%lx\n", res);
			goto errdscb;
		}
		logprintf(MLOG_INFO, "DirectSound capabilities:\n"
			  "  Flags 0x%04x\n"
			  "  SampleRate min %u max %u\n"
			  "  # Primary Buffers %u\n",
			  caps.dwFlags, caps.dwMinSecondarySampleRate, caps.dwMaxSecondarySampleRate, caps.dwPrimaryBuffers);
	}
        /* DirectSoundCapture capabilities */
	if (audioio->flags & IO_RDONLY) {
		ccaps.dwSize = sizeof(ccaps);
		if (FAILED(res = IDirectSoundCapture_GetCaps(audioio->dsrec, &ccaps))) {
			logprintf(MLOG_ERROR, "DirectSoundCaptureGetCaps error 0x%lx\n", res);
			goto errdscb;
		}
		logprintf(MLOG_INFO, "DirectSoundCapture capabilities:\n"
			  "  Flags 0x%04x\n"
			  "  Formats 0x%04x\n"
			  "  Channels %u\n",
			  ccaps.dwFlags, ccaps.dwFormats, ccaps.dwChannels);
	}
        /* adjust sampling rate */
 	if (audioio->flags & IO_WRONLY) {
		if (audioio->samplerate < caps.dwMinSecondarySampleRate)
			audioio->samplerate = caps.dwMinSecondarySampleRate;
	}
	if (audioio->flags & IO_RDONLY) {
		if (audioio->samplerate < 11025)
			audioio->samplerate = 11025;
		if (audioio->samplerate <= 22050 && (audioio->samplerate > 11025 || !(ccaps.dwFormats & WAVE_FORMAT_1M16)))
			audioio->samplerate = 22050;
		if (audioio->samplerate <= 44100 && (audioio->samplerate > 22050 || !(ccaps.dwFormats & WAVE_FORMAT_2M16)))
			audioio->samplerate = 44100;
		if (audioio->samplerate > 44100 || (audioio->samplerate == 44100 && !(ccaps.dwFormats & WAVE_FORMAT_4M16))) {
			logprintf(MLOG_ERROR, "Unsupported capture sampling rate\n");
			goto errdscb;
		}
	}
 	if (audioio->flags & IO_WRONLY) {
		if (!(caps.dwFlags & DSCAPS_PRIMARY16BIT) || !(caps.dwFlags & DSCAPS_PRIMARYMONO)) {
			logprintf(MLOG_ERROR, "Unsupported playback format 16bit mono\n");
			goto errdscb;
		}
		if (audioio->samplerate < caps.dwMinSecondarySampleRate || audioio->samplerate > caps.dwMaxSecondarySampleRate) {
			logprintf(MLOG_ERROR, "Unsupported playback sampling rate %u\n", audioio->samplerate);
			goto errdscb;
		}
	}
        /* create capture buffer */
	if (audioio->flags & IO_RDONLY) {
		memset(&waveformat, 0, sizeof(waveformat));
		waveformat.wFormatTag = WAVE_FORMAT_PCM;
		waveformat.wBitsPerSample = 16;
		waveformat.nChannels = 1;
		waveformat.nSamplesPerSec = audioio->samplerate;
		waveformat.nBlockAlign = waveformat.nChannels * waveformat.wBitsPerSample / 8;
		waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
		memset(&cbdesc, 0, sizeof(cbdesc));
		cbdesc.dwSize = sizeof(cbdesc);
		cbdesc.dwFlags = /* DSCBCAPS_WAVEMAPPED */ 0;
		cbdesc.dwBufferBytes = 65536;
		cbdesc.lpwfxFormat = &waveformat;
		if (FAILED(res = IDirectSoundCapture_CreateCaptureBuffer(audioio->dsrec, &cbdesc, &audioio->recbuf, NULL))) {
			logprintf(MLOG_ERROR, "CreateSoundCaptureBuffer error 0x%lx\n", res);
			goto errdscb;
		}
	}
        /* create playback buffer */
 	if (audioio->flags & IO_WRONLY) {
		if (isprimary) {
			memset(&bdesc, 0, sizeof(bdesc));
			bdesc.dwSize = sizeof(bdesc);
			bdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_PRIMARYBUFFER;
			bdesc.dwBufferBytes = 0;
			bdesc.lpwfxFormat = NULL;
			if (FAILED(res = IDirectSound_CreateSoundBuffer(audioio->dsplay, &bdesc, &audioio->playbuf, NULL))) {
				logprintf(MLOG_ERROR, "DirectSoundCreateSoundBuffer error 0x%lx\n", res);
				goto errdspb;
			}
			memset(&waveformat, 0, sizeof(waveformat));
			waveformat.wFormatTag = WAVE_FORMAT_PCM;
			waveformat.wBitsPerSample = 16;
			waveformat.nChannels = 1;
			waveformat.nSamplesPerSec = audioio->samplerate;
			waveformat.nBlockAlign = waveformat.nChannels * waveformat.wBitsPerSample / 8;
			waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
			if (FAILED(res = IDirectSoundBuffer_SetFormat(audioio->playbuf, &waveformat))) {
				logprintf(MLOG_ERROR, "DirectSoundBufferSetFormat error 0x%lx\n", res);
				goto errsnd;
			}
			if (FAILED(res = IDirectSoundBuffer_GetFormat(audioio->playbuf, &waveformat, sizeof(waveformat), NULL))) {
				logprintf(MLOG_ERROR, "DirectSoundBufferGetFormat error 0x%lx\n", res);
				goto errsnd;
			}
			logprintf(MLOG_INFO, "Sampling rates: Recording %u Playback %u\n", audioio->samplerate, waveformat.nSamplesPerSec);
			if (audioio->samplerate != waveformat.nSamplesPerSec) {
				logprintf(MLOG_ERROR, "sampling rates (%u,%u) too different\n", audioio->samplerate, waveformat.nSamplesPerSec);
				goto errsnd;
			}
		} else {
			/* first try to set the format of the primary buffer */
			memset(&bdesc, 0, sizeof(bdesc));
			bdesc.dwSize = sizeof(bdesc);
			bdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_PRIMARYBUFFER;
			bdesc.dwBufferBytes = 0;
			bdesc.lpwfxFormat = NULL;
			if (FAILED(res = IDirectSound_CreateSoundBuffer(audioio->dsplay, &bdesc, &audioio->playbuf, NULL))) {
				logprintf(MLOG_ERROR, "DirectSoundCreateSoundBuffer (primary) error 0x%lx\n", res);
			} else {
				memset(&waveformat, 0, sizeof(waveformat));
				waveformat.wFormatTag = WAVE_FORMAT_PCM;
				waveformat.wBitsPerSample = 16;
				waveformat.nChannels = 1;
				waveformat.nSamplesPerSec = audioio->samplerate;
				waveformat.nBlockAlign = waveformat.nChannels * waveformat.wBitsPerSample / 8;
				waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
				if (FAILED(res = IDirectSoundBuffer_SetFormat(audioio->playbuf, &waveformat))) {
					logprintf(MLOG_ERROR, "DirectSoundBufferSetFormat (primary) error 0x%lx\n", res);
				}
				IDirectSoundBuffer_Release(audioio->playbuf);        
			}
			/* create secondary */
			memset(&waveformat, 0, sizeof(waveformat));
			waveformat.wFormatTag = WAVE_FORMAT_PCM;
			waveformat.wBitsPerSample = 16;
			waveformat.nChannels = 1;
			waveformat.nSamplesPerSec = audioio->samplerate;
			waveformat.nBlockAlign = waveformat.nChannels * waveformat.wBitsPerSample / 8;
			waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
			memset(&bdesc, 0, sizeof(bdesc));
			bdesc.dwSize = sizeof(bdesc);
			bdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
			bdesc.dwBufferBytes = 65536;
			bdesc.lpwfxFormat = &waveformat;
			if (FAILED(res = IDirectSound_CreateSoundBuffer(audioio->dsplay, &bdesc, &audioio->playbuf, NULL))) {
				logprintf(MLOG_ERROR, "DirectSoundCreateSoundBuffer error 0x%lx\n", res);
				goto errdspb;
			}
		}
	}
        /* find out buffer size */
 	if (audioio->flags & IO_WRONLY) {
		bcaps.dwSize = sizeof(bcaps);
		if (FAILED(res = IDirectSoundBuffer_GetCaps(audioio->playbuf, &bcaps))) {
			logprintf(MLOG_ERROR, "DirectSoundBufferGetCaps error 0x%lx\n", res);
			goto errsnd;
		}
		logprintf(MLOG_INFO, "Playback buffer characteristics:\n"
			  "  Flags 0x%04x\n"
			  "  Buffer Bytes %u\n"
			  "  Unlock Transfer rate %u\n"
			  "  CPU overhead %u\n",
			  bcaps.dwFlags, bcaps.dwBufferBytes, bcaps.dwUnlockTransferRate, bcaps.dwPlayCpuOverhead);
		audioio->playbufsz = bcaps.dwBufferBytes;
	}
 	if (audioio->flags & IO_RDONLY) {
		cbcaps.dwSize = sizeof(cbcaps);
		if (FAILED(res = IDirectSoundCaptureBuffer_GetCaps(audioio->recbuf, &cbcaps))) {
			logprintf(MLOG_ERROR, "DirectSoundCaptureBufferGetCaps error 0x%lx\n", res);
			goto errsnd;
		}
		logprintf(MLOG_INFO, "Recording buffer characteristics:\n"
			  "  Flags 0x%04x\n"
			  "  Buffer Bytes %u\n",
			  cbcaps.dwFlags, cbcaps.dwBufferBytes);
		audioio->recbufsz = cbcaps.dwBufferBytes;
		if (FAILED(res = IDirectSoundCaptureBuffer_Start(audioio->recbuf, DSCBSTART_LOOPING))) {
			logprintf(MLOG_ERROR, "DirectSoundCaptureBufferStart error 0x%lx\n", res);
			goto errsnd;
		}
	}
	/*
	 * zero playback buffer and start it
	 */
 	if (audioio->flags & IO_WRONLY) {
		if (FAILED(res = IDirectSoundBuffer_Lock(audioio->playbuf, 0, audioio->playbufsz, &lptr1, &lbytes1, NULL, NULL, 0))) {
			if (res != DSERR_BUFFERLOST) {
				logprintf(MLOG_ERROR, "DirectSoundBufferLock error 0x%lx\n", res);
				goto errsnd;
			}
			if (FAILED(res = IDirectSoundBuffer_Restore(audioio->playbuf))) {
				logprintf(MLOG_ERROR, "DirectSoundBufferRestore error 0x%lx\n", res);
				goto errsnd;
			}
			if (FAILED(res = IDirectSoundBuffer_Lock(audioio->playbuf, 0, audioio->playbufsz, &lptr1, &lbytes1, NULL, NULL, 0))) {
				logprintf(MLOG_ERROR, "DirectSoundBufferLock error 0x%lx\n", res);
				goto errsnd;
			}
		}
		memset(lptr1, 0, lbytes1);
		if (FAILED(res = IDirectSoundBuffer_Unlock(audioio->playbuf, lptr1, lbytes1, NULL, 0))) {
			logprintf(MLOG_ERROR, "DirectSoundBufferUnlock error 0x%lx\n", res);
			goto errsnd;
		}
		if (FAILED(res = IDirectSoundBuffer_Play(audioio->playbuf, 0, 0, DSBPLAY_LOOPING))) {
			logprintf(MLOG_ERROR, "DirectSoundBufferPlay error 0x%lx\n", res);
			goto errsnd;
		}
	}
	*samplerate = audioio->samplerate;
        return &audioio->audioio;

  errsnd:
 	if (audioio->flags & IO_WRONLY) {
		IDirectSoundBuffer_Stop(audioio->playbuf);
		IDirectSoundBuffer_Release(audioio->playbuf);
	}
  errdspb:
 	if (audioio->flags & IO_RDONLY) {
		IDirectSoundCaptureBuffer_Stop(audioio->recbuf);
		IDirectSoundCaptureBuffer_Release(audioio->recbuf);
	}
  errdscb:      
 	if (audioio->flags & IO_RDONLY)
		IDirectSoundCapture_Release(audioio->dsrec);
  errdsb:
 	if (audioio->flags & IO_WRONLY)
		IDirectSound_Release(audioio->dsplay);
  errdscreate:
	free(audioio);
        return NULL;
}

/* ---------------------------------------------------------------------- */
