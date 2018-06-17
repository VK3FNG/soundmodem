/*****************************************************************************/

/*
 *	soundmodem.c  --  FlexNet driver for SoundModem.
 *
 *	Copyright (C) 1999-2000
 *          Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  History:
 *   0.1  23.06.1999  Created
 *   0.2  07.01.2000  Expanded to usbdevfs capabilities
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include "flexdrv.h"
#include "resource.h"

#include "modem.h"
#include "pttio.h"
#include "audioio.h"

/* ----------------------------------------------------------------------- */

/* note: no trailing slash! */
#define REGISTRYPATH "SOFTWARE\\FlexNet\\SoundModem"
#define REGISTRYKEY  HKEY_LOCAL_MACHINE

#define DRIVER_NAME	"SoundModem"
#define DRIVER_VERSION  "0.1"

#define MAXCHANNELS   4
#define MAXPAR       10

#define RXBUFFER_SIZE     ((MAXFLEN*6U/5U)+8U)
#define TXBUFFER_SIZE     4096U  /* must be a power of 2 and >= MAXFLEN*6/5+8; NOTE: in words */

unsigned int log_verblevel;

static struct state {
	volatile unsigned int active;
        
        unsigned int nrchannels;
       	char cfgname[64];

	unsigned int samplerate;

	L1FRAME txframe;
	pthread_mutex_t txmutex;
	pthread_cond_t txcond;
	pthread_t txthread;
	unsigned int txrunning;
	L1FRAME rxframe;
	pthread_mutex_t rxmutex;
	pthread_cond_t rxcond;
	byte pttstate;
        byte txdelay;
        byte fulldup;

	HANDLE hdevlife;

	struct modemchannel {
		char devname[64];
		unsigned int regchnum;

		unsigned int rxrunning;
		pthread_t rxthread;
		struct modulator *modch;
		struct demodulator *demodch;
		void *modstate;
		void *demodstate;

		byte dcd;
		byte mode;
		unsigned int bitrate;
		unsigned int scale;
		unsigned int calib;

		L1_STATISTICS stat;

		struct {
			unsigned rd, wr;
			unsigned char buf[TXBUFFER_SIZE];
		} htx;
        
		struct {
			unsigned int bitbuf, bitstream, numbits, state;
			unsigned char *bufptr;
			int bufcnt;
			unsigned char buf[RXBUFFER_SIZE];
		} hrx;

	} chan[MAXCHANNELS];
        struct pttio ptt;
	struct audioio *audioio;
} state = { 10, 0, };

/* ---------------------------------------------------------------------- */



/* ---------------------------------------------------------------------- */

int lprintf(unsigned vl, const char *format, ...)
{
        va_list ap;
        char buf[512];
        int r;

        if (vl > log_verblevel)
                return 0;
        va_start(ap, format);
        r = vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        OutputDebugString(buf);
        return r;
}

void logvprintf(unsigned int level, const char *fmt, va_list args)
{
        char buf[512];
        
        if (level > log_verblevel)
                return;
        vsnprintf(buf, sizeof(buf), fmt, args);
        OutputDebugString(buf);
        if (!level)
                exit(1);
}

void logprintf(unsigned int level, const char *fmt, ...)
{
        va_list args;

        va_start(args, fmt);
        logvprintf(level, fmt, args);
        va_end(args);
}

/* ---------------------------------------------------------------------- */
/*
 * the CRC routines are stolen from WAMPES
 * by Dieter Deyke
 */

const u_int16_t crc_ccitt_table[0x100] = {
        0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
        0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
        0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
        0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
        0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
        0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
        0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
        0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
        0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
        0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
        0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
        0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
        0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
        0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
        0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
        0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
        0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
        0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
        0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
        0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
        0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
        0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
        0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
        0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
        0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
        0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
        0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
        0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
        0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
        0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
        0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
        0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/* ---------------------------------------------------------------------- */

static inline u_int16_t calc_crc_ccitt(const u_int8_t *buffer, int len)
{
        u_int16_t crc = 0xffff;

        for (;len>0;len--)
                crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ *buffer++) & 0xff];
        crc ^= 0xffff;
	return crc;
}

static inline void append_crc_ccitt(u_int8_t *buffer, int len)
{
        u_int16_t crc = calc_crc_ccitt(buffer, len);
        buffer[len] = crc;
        buffer[len+1] = crc >> 8;
}

static inline int check_crc_ccitt(const u_int8_t *buffer, int len)
{
        u_int16_t crc = calc_crc_ccitt(buffer, len);
        return (crc & 0xffff) == 0x0f47;
}

/* ---------------------------------------------------------------------- */

/*
 * high performance HDLC encoder
 * yes, it's ugly, but generates pretty good code
 */

#define ENCODEITERA(j)                         \
({                                             \
        if (!(notbitstream & (0x1f0 << j)))    \
                goto stuff##j;                 \
  encodeend##j:                                \
})

#define ENCODEITERB(j)                                          \
({                                                              \
  stuff##j:                                                     \
        bitstream &= ~(0x100 << j);                             \
        bitbuf = (bitbuf & (((2 << j) << numbit) - 1)) |        \
                ((bitbuf & ~(((2 << j) << numbit) - 1)) << 1);  \
        numbit++;                                               \
        notbitstream = ~bitstream;                              \
        goto encodeend##j;                                      \
})

static void hdlc_encode(struct modemchannel *chan, unsigned char *pkt, unsigned int len)
{
        unsigned bitstream, notbitstream, bitbuf, numbit;
	unsigned wr = chan->htx.wr;

	append_crc_ccitt(pkt, len);
	len += 2;
	bitstream = 0;
	bitbuf = 0x7e;
	numbit = 8; /* opening flag */
	while (numbit >= 8) {
		chan->htx.buf[wr] = bitbuf;
		wr = (wr + 1) % TXBUFFER_SIZE;
		if (wr == chan->htx.rd)
			*(int *)0 = 0;  /* must not happen! */
		bitbuf >>= 8;
		numbit -= 8;
	}
 	for (; len > 0; len--, pkt++) {
		bitstream >>= 8;
		bitstream |= ((unsigned int)*pkt) << 8;
		bitbuf |= ((unsigned int)*pkt) << numbit;
		notbitstream = ~bitstream;
		ENCODEITERA(0);
		ENCODEITERA(1);
		ENCODEITERA(2);
		ENCODEITERA(3);
		ENCODEITERA(4);
		ENCODEITERA(5);
		ENCODEITERA(6);
		ENCODEITERA(7);
		goto enditer;
		ENCODEITERB(0);
		ENCODEITERB(1);
		ENCODEITERB(2);
		ENCODEITERB(3);
		ENCODEITERB(4);
		ENCODEITERB(5);
		ENCODEITERB(6);
		ENCODEITERB(7);
	enditer:
		numbit += 8;
		while (numbit >= 8) {
			chan->htx.buf[wr] = bitbuf;
			wr = (wr + 1) % TXBUFFER_SIZE;
			if (wr == chan->htx.rd)
				*(int *)0 = 0;  /* must not happen! */
			bitbuf >>= 8;
			numbit -= 8;
		}
	}
	bitbuf |= 0x7e7e << numbit;
	numbit += 16;
	while (numbit >= 8) {
		chan->htx.buf[wr] = bitbuf;
		wr = (wr + 1) % TXBUFFER_SIZE;
		if (wr == chan->htx.rd)
			*(int *)0 = 0;  /* must not happen! */
		bitbuf >>= 8;
		numbit -= 8;
	}
	chan->htx.wr = wr;
}

/* ---------------------------------------------------------------------- */

static void rx_packet(struct modemchannel *chan, const unsigned char *pkt, unsigned int len)
{
	unsigned int kanal;

	for (kanal = 0; kanal < state.nrchannels && chan != &state.chan[kanal]; kanal++);
	chan->stat.rx_frames++;
	pthread_mutex_lock(&state.rxmutex);
	for (;;) {
		if (!state.active) {
			pthread_mutex_unlock(&state.rxmutex);
			pthread_exit(NULL);
		}
		if (!state.rxframe.len)
			break;
		pthread_cond_wait(&state.rxcond, &state.rxmutex);
	}
	memcpy(state.rxframe.frame, pkt, len);
	state.rxframe.len = len;
	state.rxframe.txdelay = 0;
	state.rxframe.kanal = kanal;
	pthread_mutex_unlock(&state.rxmutex);
	pthread_cond_broadcast(&state.rxcond);
}

static void do_rxpacket(struct modemchannel *chan)
{
	if (chan->hrx.bufcnt < 3 || chan->hrx.bufcnt > MAXFLEN) 
		return;
	if (!check_crc_ccitt(chan->hrx.buf, chan->hrx.bufcnt)) 
		return;
	rx_packet(chan, chan->hrx.buf, chan->hrx.bufcnt-2);
}

#define DECODEITERA(j)                                                        \
({                                                                            \
        if (!(notbitstream & (0x0fc << j)))              /* flag or abort */  \
                goto flgabrt##j;                                              \
        if ((bitstream & (0x1f8 << j)) == (0xf8 << j))   /* stuffed bit */    \
                goto stuff##j;                                                \
  enditer##j:                                                                 \
})

#define DECODEITERB(j)                                                                 \
({                                                                                     \
  flgabrt##j:                                                                          \
        if (!(notbitstream & (0x1fc << j))) {              /* abort received */        \
                state = 0;                                                             \
                goto enditer##j;                                                       \
        }                                                                              \
        if ((bitstream & (0x1fe << j)) != (0x0fc << j))   /* flag received */          \
                goto enditer##j;                                                       \
        if (state)                                                                     \
                do_rxpacket(chan);                                                     \
        chan->hrx.bufcnt = 0;                                                          \
        chan->hrx.bufptr = chan->hrx.buf;                                              \
        state = 1;                                                                     \
        numbits = 7-j;                                                                 \
        goto enditer##j;                                                               \
  stuff##j:                                                                            \
        numbits--;                                                                     \
        bitbuf = (bitbuf & ((~0xff) << j)) | ((bitbuf & ~((~0xff) << j)) << 1);        \
        goto enditer##j;                                                               \
})
        
static inline void hdlc_receive(struct modemchannel *chan, const unsigned char *data, unsigned nrbytes)
{
	unsigned bits, bitbuf, notbitstream, bitstream, numbits, state;

	/* start of HDLC decoder */
	numbits = chan->hrx.numbits;
	state = chan->hrx.state;
	bitstream = chan->hrx.bitstream;
	bitbuf = chan->hrx.bitbuf;
	while (nrbytes > 0) {
		bits = *data++;
		nrbytes--;
		bitstream >>= 8;
		bitstream |= ((unsigned int)bits) << 8;
		bitbuf >>= 8;
		bitbuf |= ((unsigned int)bits) << 8;
		numbits += 8;
		notbitstream = ~bitstream;
		DECODEITERA(0);
		DECODEITERA(1);
		DECODEITERA(2);
		DECODEITERA(3);
		DECODEITERA(4);
		DECODEITERA(5);
		DECODEITERA(6);
		DECODEITERA(7);
		goto enddec;
		DECODEITERB(0);
		DECODEITERB(1);
		DECODEITERB(2);
		DECODEITERB(3);
		DECODEITERB(4);
		DECODEITERB(5);
		DECODEITERB(6);
		DECODEITERB(7);
	enddec:
		while (state && numbits >= 8) {
			if (chan->hrx.bufcnt >= RXBUFFER_SIZE) {
				state = 0;
			} else {
				*(chan->hrx.bufptr)++ = bitbuf >> (16-numbits);
				chan->hrx.bufcnt++;
				numbits -= 8;
			}
		}
	}
        chan->hrx.numbits = numbits;
	chan->hrx.state = state;
	chan->hrx.bitstream = bitstream;
	chan->hrx.bitbuf = bitbuf;
}

/* ---------------------------------------------------------------------- */

void p3dreceive(struct modemchannel *chan, const unsigned char *pkt, u_int16_t crc)
{
	unsigned char buf[256+16];
	unsigned int i;

	if (crc)
		return;
	buf[0] = 'Q' << 1;
	buf[1] = 'S' << 1;
	buf[2] = 'T' << 1;
	buf[3] = ' ' << 1;
	buf[4] = ' ' << 1;
	buf[5] = ' ' << 1;
	buf[6] = (0 << 1) | 0x80;
	buf[7] = 'A' << 1;
	buf[8] = 'O' << 1;
	buf[9] = '4' << 1;
	buf[10] = '0' << 1;
	buf[11] = ' ' << 1;
	buf[12] = ' ' << 1;
	buf[13] = (1 << 1) | 1;
	buf[14] = 0x03; /* UI */
	buf[15] = 0xf0; /* PID */
	memcpy(&buf[16], &pkt[0], 256);
	rx_packet(chan, buf, 256+16);
	buf[13] = (2 << 1) | 1;
	memcpy(&buf[16], &pkt[256], 256);
	rx_packet(chan, buf, 256+16);
}

void p3drxstate(struct modemchannel *chan, unsigned int synced, unsigned int carrierfreq)
{
	pktsetdcd(chan, !!synced);
}

/* ---------------------------------------------------------------------- */

static void countchannels(const char *cfgname, unsigned int maxch)
{
        char name[256];
        HKEY regkey;
        LONG err;
        DWORD len;
        DWORD index = 0;
	unsigned int chcnt = 0, tmp;
	unsigned int chan[MAXCHANNELS];

        memset(chan, 0, sizeof(chan));
        if (cfgname && cfgname[0]) {
                snprintf(name, sizeof(name), "%s\\%s", REGISTRYPATH, cfgname);
                if ((err = RegOpenKeyEx(REGISTRYKEY, name, 0, KEY_READ, &regkey)) != ERROR_SUCCESS) {
                        lprintf(10, "RegOpenKeyEx(%s) returned 0x%lx\n", name, err);
                } else {
                        while (chcnt < MAXCHANNELS && chcnt < maxch) {
                                len = sizeof(name);
                                if ((RegEnumKeyEx(regkey, index, name, &len, NULL, NULL, NULL, NULL)) != ERROR_SUCCESS)
                                        break;
                                lprintf(9, "soundmodem: cfg %s: index %u name %s len %u\n", cfgname, index, name, len);
                                index++;
                                if (!len)
                                        continue;
                                if (name[0] >= '0' && name[0] <= '9')
                                        chan[chcnt++] = strtoul(name, NULL, 0);
                        }
                        RegCloseKey(regkey);
                }
        }
        if (!chcnt)
                chcnt = 1;
	state.nrchannels = chcnt;
	/* bubble sort */
	do {
		for (len = index = 0; index < chcnt-1; index++) {
			if (chan[index] > chan[index+1]) {
				tmp = chan[index];
				chan[index] = chan[index+1];
				chan[index+1] = tmp;
				len = 1;
			}
		}
        } while (len);
	/* do actual store */
	for (index = 0; index < chcnt; index++)
		state.chan[index].regchnum = chan[index];
}

static int getprop(int ch, const char *typname, const char *propname, char *result, unsigned int reslen)
{
        HKEY key;
        DWORD err, vtype, len = reslen;
        char name[256];

        if (!state.cfgname || !state.cfgname[0])
                return -1;
	if (ch >= 0 && ch < MAXCHANNELS)
		snprintf(name, sizeof(name), "%s\\%s\\%u\\%s", REGISTRYPATH, state.cfgname, state.chan[ch].regchnum, typname);
	else
		snprintf(name, sizeof(name), "%s\\%s\\%s", REGISTRYPATH, state.cfgname, typname);
	result[0] = 0;
	if ((err = RegCreateKeyEx(REGISTRYKEY, name, 0, "", REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &key, NULL)) != ERROR_SUCCESS) {
                lprintf(10, "RegCreateKeyEx(%s) returned 0x%lx\n", name, err);
                return -1;
        }
        err = RegQueryValueEx(key, propname, NULL, &vtype, result, &len);
        RegCloseKey(key);
        if (err != ERROR_SUCCESS) {
                lprintf(10, "RegQueryValueEx(%s) returned 0x%lx\n", propname, err);
		result[0] = 0;
                return -1;
        }
        if (vtype != REG_SZ) {
		result[0] = 0;
                return -1;
 	}
        if (len >= reslen)
                len = reslen-1;
        result[len] = 0;
        return len;
}

/* ---------------------------------------------------------------------- */

void audiowrite(struct modemchannel *chan, const int16_t *samples, unsigned int nr)
{
	if (!state.audioio->write)
		return;
	state.audioio->write(state.audioio, samples, nr);
}

void audioread(struct modemchannel *chan, int16_t *samples, unsigned int nr, u_int16_t tim)
{
	if (!state.audioio->read) {
		pthread_exit(NULL);
		return;
	}
	state.audioio->read(state.audioio, samples, nr, tim);
}

u_int16_t audiocurtime(struct modemchannel *chan)
{
	if (!state.audioio->curtime)
		return 0;
	return state.audioio->curtime(state.audioio);
}

/* --------------------------------------------------------------------- */

int pktget(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
	unsigned int j, n = len;

	pthread_mutex_lock(&state.txmutex);
	if (!state.active) {
		pthread_mutex_unlock(&state.txmutex);
		state.pttstate = 0;
		pttsetptt(&state.ptt, 0);
                lprintf(10, "soundmodem: tx terminate (pktget)\n");
		pthread_exit(NULL);
	}
	if (chan->htx.rd == chan->htx.wr) {
		if (!chan->calib) {
			pthread_mutex_unlock(&state.txmutex);
			return 0;
		}
		/* check first if anyone else has a real packet to send */
		for (j = 0; j < state.nrchannels; j++)
			if (state.chan[j].htx.rd != state.chan[j].htx.wr) {
				pthread_mutex_unlock(&state.txmutex);
				return 0;
			}
		/* fill calib */
		if (chan->calib < n) {
			n = chan->calib;
			chan->calib = 0;
		} else
			chan->calib -= n;
		memset(data, 0, n);
		pthread_mutex_unlock(&state.txmutex);
		return n;
	}
	while (n > 0) {
		if (chan->htx.wr >= chan->htx.rd)
			j = chan->htx.wr - chan->htx.rd;
		else
			j = TXBUFFER_SIZE - chan->htx.rd;
		if (j > n)
			j = n;
		if (!j)
			break;
		memcpy(data, &chan->htx.buf[chan->htx.rd], j);
		data += j;
		n -= j;
		chan->htx.rd = (chan->htx.rd + j) % TXBUFFER_SIZE;
	}
	if (n > 0)
		memset(data, 0, n);
	pthread_mutex_unlock(&state.txmutex);
	return (len - n);
}

void pktput(struct modemchannel *chan, const unsigned char *data, unsigned int len)
{
	hdlc_receive(chan, data, len);
}

void pktsetdcd(struct modemchannel *chan, int dcd)
{
	chan->dcd = !!dcd;
}

/* --------------------------------------------------------------------- */

/* search for next channel with something to transmit; prefer channels with real packets over channels calibrating */
static int searchnextch(int ch)
{
	unsigned int i;
	int calibch = -1;
	
	if (!state.audioio->write)
		return -1;
	if (ch < 0 || ch >= state.nrchannels)
		ch = state.nrchannels-1;
        i = ch;
	do {
                i++;
		if (i >= state.nrchannels)
			i = 0;
		if (!state.chan[i].modch || !state.chan[i].modch->modulate)
			continue;
		if (state.chan[i].htx.rd != state.chan[i].htx.wr)
			return i;
		if (state.chan[i].calib > 0 && calibch == -1)
			calibch = i;
	} while (i != ch);
	return calibch;
}

static void *transmitter(void *dummy)
{
	int curch;

        lprintf(10, "soundmodem: tx start\n");
	state.pttstate = 0;
	pttsetptt(&state.ptt, 0);
        lprintf(10, "soundmodem: acq mutex\n");
	pthread_mutex_lock(&state.txmutex);
        lprintf(10, "soundmodem: have mutex\n");
	for (;;) {
		if (!state.active) {
			pthread_mutex_unlock(&state.txmutex);
                        lprintf(10, "soundmodem: tx terminate\n");
			return NULL;
		}
		curch = searchnextch(-1);
		if (curch < 0) {
                        lprintf(10, "soundmodem: tx wait\n");
			pthread_cond_wait(&state.txcond, &state.txmutex);
                        lprintf(10, "soundmodem: tx wakeup\n");
			continue;
		}
		pthread_mutex_unlock(&state.txmutex);
                lprintf(10, "soundmodem: tx keying up\n");
		pttsetptt(&state.ptt, 1);
		if (state.audioio->transmitstart)
			state.audioio->transmitstart(state.audioio);
		state.pttstate = CH_PTT;
                lprintf(10, "soundmodem: tx channel %u (txdelay)\n", curch);
		state.chan[curch].modch->modulate(state.chan[curch].modstate, state.txdelay * 10);
		while (state.active) {
			pthread_mutex_lock(&state.txmutex);
			if (!state.active)
				break;
			curch = searchnextch(curch);
			if (curch < 0)
				break;
			pthread_mutex_unlock(&state.txmutex);
                        lprintf(10, "soundmodem: tx channel %u\n", curch);
                        state.chan[curch].modch->modulate(state.chan[curch].modstate, 0);
		}
		pthread_mutex_unlock(&state.txmutex);
                if (state.audioio->transmitstop)
			state.audioio->transmitstop(state.audioio);
		state.pttstate = 0;
		pttsetptt(&state.ptt, 0);
		pthread_mutex_lock(&state.txmutex);
                lprintf(10, "soundmodem: tx switching off\n");
	}
}

static void *receiver(void *__chan)
{
	struct modemchannel *chan = __chan;

	chan->demodch->demodulate(chan->demodstate);
	return NULL;
}

/* --------------------------------------------------------------------- */

static void start(void)
{
        struct modulator *modch;
        struct demodulator *demodch;
	char params[MAXPAR][64];
	const char *parptr[MAXPAR];
	const struct modemparams *par;
	struct modemchannel *chan;
	char buf[128];
	unsigned int i, j, mode = 0;
	unsigned int samplerate = 5000;  /* minimum sampling rate */

	if (state.active)
		return;
        lprintf(5, "soundmodem: start\n");
	pthread_mutex_init(&state.rxmutex, NULL);
	pthread_cond_init(&state.rxcond, NULL);
	state.rxframe.len = 0;
	pthread_mutex_init(&state.txmutex, NULL);
	pthread_cond_init(&state.txcond, NULL);
        /* initialize txdelay and fullduplex */
        state.txdelay = 10;
        state.fulldup = 0;
        if (getprop(-1, "chaccess", "fulldup", params[0], sizeof(params[0])) >= 1 && params[0][0] == '1')
                state.fulldup = 1;
        if (getprop(-1, "chaccess", "txdelay", params[0], sizeof(params[0])) >= 1)
                state.txdelay = strtoul(params[0], NULL, 0) / 10;
        /* initialize channel variables */
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		chan->dcd = 0;
		chan->calib = 0;
	}
	for (i = 0; i < state.nrchannels; i++) {
		state.chan[i].modch = NULL;
		if (getprop(i, "mod", "mode", buf, sizeof(buf)) < 1)
			continue;
		for (modch = &afskmodulator; modch && strcmp(modch->name, buf); modch = modch->next);
		state.chan[i].modch = modch;
	}
	for (i = 0; i < state.nrchannels; i++) {
		state.chan[i].demodch = NULL;
		if (getprop(i, "demod", "mode", buf, sizeof(buf)) < 1)
			continue;
		for (demodch = &afskdemodulator; demodch && strcmp(demodch->name, buf); demodch = demodch->next);
		state.chan[i].demodch = demodch;
	}
	/* configure modems */
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
                lprintf(5, "Channel: %u  Modulator %s Demodulator %s\n", i,
                        (chan->modch && chan->modch->name) ? chan->modch->name : "-",
                        (chan->demodch && chan->demodch->name) ? chan->demodch->name : "-");                        
		/* modulator */
		if (chan->modch && chan->modch->config) {
			memset(parptr, 0, sizeof(parptr));
			if ((par = chan->modch->params))
				for (j = 0; j < MAXPAR && par->name; j++, par++) {
					if (getprop(i, "mod", par->name, params[j], sizeof(params[j])) < 1)
						params[j][0] = 0;
					else
						parptr[j] = params[j];
                                        lprintf(6, "Modulator Parameter %s: %s\n", par->name, params[j]);
				}
			j = samplerate;
			chan->modstate = chan->modch->config(chan, &j, parptr);
			if (j > samplerate)
				samplerate = j;
			if (chan->modch->modulate)
				mode |= IO_WRONLY;
		}
		/* demodulator */
		if (chan->demodch && chan->demodch->config) {
			memset(parptr, 0, sizeof(parptr));
			if ((par = chan->demodch->params))
				for (j = 0; j < MAXPAR && par->name; j++, par++) {
					if (getprop(i, "demod", par->name, params[j], sizeof(params[j])) < 1)
						params[j][0] = 0;
					else
						parptr[j] = params[j];
                                        lprintf(6, "Demodulator Parameter %s: %s\n", par->name, params[j]);
				}
			j = samplerate;
			chan->demodstate = chan->demodch->config(chan, &j, parptr);
			if (j > samplerate)
				samplerate = j;
			if (chan->demodch->demodulate)
				mode |= IO_RDONLY;
		}
                snprintf(chan->devname, sizeof(chan->devname), "SoundModem (%s,%s)", 
                         (chan->modch && chan->modch->name) ? chan->modch->name : "-",
                         (chan->demodch && chan->demodch->name) ? chan->demodch->name : "-");
	}
	lprintf(5, "Minimum Samplingrate: %u\n", samplerate);
	/* start audio */
	memset(parptr, 0, sizeof(parptr));
	par = ioparams_soundcard;
	for (j = 0; j < MAXPAR && par->name; j++, par++) {
		if (getprop(-1, "audio", par->name, params[j], sizeof(params[j])) < 1)
			params[j][0] = 0;
		else
			parptr[j] = params[j];
                lprintf(6, "Audio Parameter %s: %s\n", par->name, params[j]);
        }
	if (!(state.audioio = ioopen_soundcard(&samplerate, mode, parptr))) {
		lprintf(3, "Cannot start DirectX IO\n");
		goto err;
	}
	/* start modems */
	lprintf(5, "Real Samplingrate: %u\n", samplerate);
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		if (chan->modch && chan->modch->init)
			chan->modch->init(chan->modstate, samplerate);
		chan->bitrate = 0;
		if (chan->demodch && chan->demodch->init)
			chan->demodch->init(chan->demodstate, samplerate, &chan->bitrate);
		if (chan->bitrate)
			chan->scale = 614400/chan->bitrate;
		else
			chan->scale = 0;
	}
	/* start PTT */
	memset(parptr, 0, sizeof(parptr));
	par = pttparams;
	for (j = 0; j < MAXPAR && par->name; j++, par++) {
		if (getprop(-1, "ptt", par->name, params[j], sizeof(params[j])) < 1)
			params[j][0] = 0;
		else
			parptr[j] = params[j];
                lprintf(6, "PTT Parameter %s: %s\n", par->name, params[j]);
        }
	if (pttinit(&state.ptt, parptr))
                lprintf(1, "cannot start PTT output\n");
	/* start receiver threads */
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		chan->rxrunning = 0;
		if (!chan->demodch || !chan->demodch->demodulate)
			continue;
		if (pthread_create(&chan->rxthread, NULL, receiver, chan)) {
			lprintf(1, "Cannot start receiver thread\n");
			continue;
		}
		chan->rxrunning = 1;
	}
	if (pthread_create(&state.txthread, NULL, transmitter, NULL))
		lprintf(1, "Cannot start receiver thread\n");
	else
		state.txrunning = 1;
	state.active = 1;
        SetEvent(state.hdevlife);
        lprintf(5, "soundmodem: running\n");
	return;

  err:
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		if (chan->modch && chan->modch->free)
			chan->modch->free(chan->modstate);
		if (chan->demodch && chan->demodch->free)
			chan->demodch->free(chan->demodstate);
	}
}

static void stop(void)
{
	struct modemchannel *chan;
	unsigned int i;

	if (!state.active)
		return;
        lprintf(5, "soundmodem: stop\n");
	state.active = 0;
	if (state.audioio->terminateread)
		state.audioio->terminateread(state.audioio);
	pthread_cond_broadcast(&state.txcond);
	pthread_cond_broadcast(&state.rxcond);
	if (state.txrunning) {
		lprintf(9, "Joining TX Thread\n");
		pthread_join(state.txthread, NULL);
		state.txrunning = 0;
	}
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		if (!chan->rxrunning)
			continue;
		lprintf(9, "Joining RX Thread %u\n", i);
		pthread_join(chan->rxthread, NULL);
		chan->rxrunning = 0;
	}
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		if (chan->modch && chan->modch->free)
			chan->modch->free(chan->modstate);
		if (chan->demodch && chan->demodch->free)
			chan->demodch->free(chan->demodstate);
	}
	if (state.audioio)
		state.audioio->release(state.audioio);
        pttsetptt(&state.ptt, 0);
        pttrelease(&state.ptt);
        SetEvent(state.hdevlife);
}

/* --------------------------------------------------------------------- */

/*
 * Treiber-Init. Sofern mehrfach aufgerufen, kommt vorher jeweils l1_exit()
 * Hier also alle Ressourcen allokieren, aber noch nicht starten sofern
 * dazu die Parameter gebraucht werden. Die kommen spaeter per l1_init_kanal()
 */

int init_device(HKEY hKey)
{
	DWORD regtype, reglen, regval;
	struct modemchannel *chan;
	unsigned int i;

        afskmodulator.next = &fskmodulator;
        afskdemodulator.next = &fskdemodulator;
        fskmodulator.next = &pammodulator;
        fskdemodulator.next = &fskpspdemodulator;
        fskpspdemodulator.next = &pamdemodulator;
        pammodulator.next = &pskmodulator;
        pamdemodulator.next = &pskdemodulator;
	pskmodulator.next = &newqpskmodulator;
	pskdemodulator.next = &newqpskdemodulator;
	newqpskdemodulator.next= &p3ddemodulator;
	pthread_mutex_init(&state.rxmutex, NULL);
	pthread_cond_init(&state.rxcond, NULL);
	state.rxframe.len = 0;
	pthread_mutex_init(&state.txmutex, NULL);
	pthread_cond_init(&state.txcond, NULL);
	reglen = sizeof(regval);
	if (RegQueryValueEx(hKey, "VerboseLevel", NULL, &regtype, (void *)&regval, &reglen) != ERROR_SUCCESS || regtype != REG_DWORD)
		regval = 10;
	log_verblevel = regval;
	reglen = sizeof(state.cfgname);
	if (RegQueryValueEx(hKey, "ConfigName", NULL, &regtype, state.cfgname, &reglen) != ERROR_SUCCESS || regtype != REG_SZ)
		state.cfgname[0] = 0;
	state.cfgname[sizeof(state.cfgname)-1] = 0;
	reglen = sizeof(regval);
	if (RegQueryValueEx(hKey, "NrChannels", NULL, &regtype, (void *)&regval, &reglen) != ERROR_SUCCESS || regtype != REG_DWORD)
		regval = 15;
        if (regval < 1)
                regval = 1;
	countchannels(state.cfgname, regval);
	state.active = 0;
        ioinit_soundcard();
	state.hdevlife = CreateEvent(NULL, FALSE, FALSE, "FlexNet Device State");
        /* initialize channel variables */
	for (i = 0; i < state.nrchannels; i++) {
		chan = &state.chan[i];
		chan->mode = MODE_off;
	}
        lprintf(5, "soundmodem: init_device: vl %u CfgName %s\n", log_verblevel, state.cfgname);
        return 1;
}

/* ------------------------------------------------------------------------- */

void l1_exit(HKEY hKey)
{
	DWORD vl = log_verblevel, nrchan = state.nrchannels;

        stop();
	RegSetValueEx(hKey, "ConfigName", 0, REG_SZ, state.cfgname, strlen(state.cfgname)+1);
	RegSetValueEx(hKey, "VerboseLevel", 0, REG_DWORD, (void *)&vl, sizeof(vl));
	RegSetValueEx(hKey, "NrChannels", 0, REG_DWORD, (void *)&nrchan, sizeof(nrchan));
	CloseHandle(state.hdevlife);
        lprintf(5, "soundmodem: l1_exit: vl %u CfgName %s\n", log_verblevel, state.cfgname);
}

/* ------------------------------------------------------------------------- */

byte *config_info(byte kanal)
{
	if (kanal >= state.nrchannels)
		return "invalid channel";
	return state.chan[kanal].devname;
}

/* ------------------------------------------------------------------------- */

static BOOL CALLBACK EdParmDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        HWND hcombo;
        HKEY regkey;
        LONG err;
        DWORD index;
        DWORD len;
        char buf[32];
	unsigned int restart, i;
        int cursel;

	switch (uMsg) {
	case WM_INITDIALOG:
                hcombo = GetDlgItem(hDlg, IDC_CONFIG);
                err = RegOpenKeyEx(REGISTRYKEY, REGISTRYPATH, 0, KEY_READ, &regkey);
                if (err == ERROR_SUCCESS) {
                        cursel = -1;
                        for (i = index = 0;; index++) {
                                len = sizeof(buf);
                                if ((RegEnumKeyEx(regkey, index, buf, &len, NULL, NULL, NULL, NULL)) != ERROR_SUCCESS)
                                        break;
                                SendMessage(hcombo, CB_ADDSTRING, 0, (LPARAM)buf);
                                if (!strcmp(buf, state.cfgname))
                                        cursel = i;
                                i++;
                        }
                        ComboBox_SetCurSel(hcombo, cursel);
                        RegCloseKey(regkey);
                }
		SetWindowText(hDlg, DRIVER_NAME" Configuration");
		break;
                
        case WM_COMMAND:
                switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDCANCEL:
                        EndDialog(hDlg, 0);
                        break;

                case IDOK:
                        GetDlgItemText(hDlg, IDC_CONFIG, buf, sizeof(buf));
                        restart = strcmp(buf, state.cfgname);
                        strncpy(state.cfgname, buf, sizeof(state.cfgname));
                        EndDialog(hDlg, restart);
                        break;
                        
                default:	
                        break;
                }
                break;
	
	default:
		return FALSE;
	}
	return TRUE;
}

/* ------------------------------------------------------------------------- */

int config_device(byte max_channels, HWND hDlg, byte channel)
{
	int restart;

	restart = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hDlg, EdParmDlgProc, 0);
	if (restart)
		countchannels(state.cfgname, max_channels);
	return restart;
}

/* ------------------------------------------------------------------------- */

u16 l1_get_ch_cnt(void)
{
	return state.nrchannels;
}

/* ------------------------------------------------------------------------- */

byte get_txdelay(byte kanal)
{
        return state.txdelay;
}

void set_txdelay(byte kanal, byte delay)
{
}

u16 get_mode(byte kanal)
{
	if (kanal >= state.nrchannels)
		return 0;
	return (state.chan[kanal].mode & ~(MODE_d|MODE_off)) | (state.fulldup ? MODE_d : 0);
}

u16 get_baud(byte kanal)
{
	if (kanal >= state.nrchannels)
		return 0;
	return state.chan[kanal].bitrate / 100;
}

/* ------------------------------------------------------------------------- */

byte l1_init_kanal(byte kanal, u16 chbaud, u16 chmode)
{
	unsigned int i, j;

        lprintf(5, "l1_init_kanal(%u,%u,0x%x)\n", kanal, chbaud, chmode);
	if (kanal >= state.nrchannels)
		return 0;
	if (chmode & MODE_off)
		state.chan[kanal].mode |= MODE_off;
	else
		state.chan[kanal].mode = chmode;
	j = MODE_off;
	for (i = 0; i < state.nrchannels; i++)
		j &= state.chan[i].mode;
	if (j & MODE_off)
		stop();
	else
		start();
	return state.active;
}

/* ------------------------------------------------------------------------- */

byte l1_ch_active(byte kanal)
{
	if (kanal >= state.nrchannels)
		return 0;
	return state.active;
}

/* ------------------------------------------------------------------------- */

char far *l1_ident(byte kanal)
{
	return DRIVER_NAME;
}

/* ------------------------------------------------------------------------- */

char far *l1_version(byte kanal)
{
	return DRIVER_VERSION;
}

/* ------------------------------------------------------------------------- */

L1_STATISTICS far *l1_stat(byte kanal, byte delete)
{
	L1_STATISTICS *p;

	if (kanal >= state.nrchannels)
		return NULL;
	p = &state.chan[kanal].stat;
     	if (delete)
		p->tx_error = p->rx_overrun = p->rx_bufferoverflow =
		p->tx_frames = p->rx_frames = p->io_error = 0;
	return p;
}

/* ------------------------------------------------------------------------- */

void set_led(byte kanal, byte ledcode)
{
#if 0
	if (state.active)
		pkt_setled(state.pkt, ledcode);
#endif
}

/* ------------------------------------------------------------------------- */

byte l1_ch_state(byte kanal)
{
	unsigned int i;
	byte st = state.pttstate;

	if (kanal >= state.nrchannels)
		return CH_DEAD;
	if (!state.active || !state.txrunning)
		return CH_DEAD;
	for (i = 0; i < state.nrchannels; i++)
		if (state.chan[i].dcd) {
			st |= CH_DCD;
			break;
		}
	if (state.rxframe.len)
		st |= CH_RXB;
	return st;
}

/* ------------------------------------------------------------------------- */

u16 l1_scale(byte kanal)
{
	if (kanal >= state.nrchannels)
		return 0;
	return state.chan[kanal].scale;
}

/* ------------------------------------------------------------------------- */

void l1_tx_calib(byte kanal, byte minutes)
{
	if (kanal >= state.nrchannels || !state.active)
		return;
	state.chan[kanal].calib = minutes * state.chan[kanal].bitrate * 60 / 8;
}

/* ------------------------------------------------------------------------- */

L1FRAME far *l1_get_framebuf(byte kanal)
{
	return &state.txframe;
}

byte l1_tx_frame(void)
{
	struct modemchannel *chan;
	unsigned int i;

	if (state.txframe.kanal >= state.nrchannels || !state.active)
		return 1;
	chan = &state.chan[state.txframe.kanal];
	if (!chan->modch || !chan->modch->modulate)
		return 1;
	pthread_mutex_lock(&state.txmutex);
        i = (TXBUFFER_SIZE - 1 + chan->htx.rd - chan->htx.wr) % TXBUFFER_SIZE;
	if (i < state.txframe.len*6/5+32) {
		pthread_mutex_unlock(&state.txmutex);
		return 0;
	}
	hdlc_encode(chan, state.txframe.frame, state.txframe.len);
	pthread_mutex_unlock(&state.txmutex);
	pthread_cond_broadcast(&state.txcond);
	chan->stat.tx_frames++;
	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * RX-Thread. Wartet auf RX-Bytes, verarbeitet sie und returned wenn
 * Handle zu oder Paket komplett
 */

L1FRAME far *l1_rx_frame(void)
{
	pthread_mutex_lock(&state.rxmutex);
	if (!state.active) {
		pthread_mutex_unlock(&state.rxmutex);
		return NULL;
	}
	for (;;) {
		state.rxframe.len = 0;
		pthread_cond_broadcast(&state.rxcond);
		pthread_cond_wait(&state.rxcond, &state.rxmutex);
		if (!state.active) {
			pthread_mutex_unlock(&state.rxmutex);
			return NULL;
		}
		if (state.rxframe.len) {
			pthread_mutex_unlock(&state.rxmutex);
			return &state.rxframe;
		}
	}
}

/* ------------------------------------------------------------------------- */
