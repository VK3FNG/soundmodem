/*****************************************************************************/

/*
 *      snprintpkt.c  --  Print an AX.25 packet (with header) into a buffer.
 *
 *      Copyright (C) 1999  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *        Swiss Federal Institute of Technology (ETH), Electronics Lab
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
 *
 *  This is the Linux realtime sound output driver
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "modem.h"

/* --------------------------------------------------------------------- */

#define ADDCH(c)             \
do {                         \
	if (p + 1 >= endp)   \
		return -1;   \
	*p++ = (c);          \
} while(0)

#define ADDSTR(s)            \
do {                         \
	int l = strlen(s);   \
	if (p + l >= endp)   \
		return -1;   \
	memcpy(p, s, l);     \
	p += l;              \
} while(0)

#ifdef HAVE_SNPRINTF

#define ADDF(fmt, args...)                            \
do {                                                  \
	int xlen = snprintf(p, endp-p, fmt, ## args); \
	if (xlen < 0)                                 \
		return -1;                            \
	p += xlen;                                    \
} while(0)

#else /* HAVE_SNPRINTF */

#define ADDF(fmt, args...)                            \
do {                                                  \
	char xbuf[64];				      \
	int xlen = sprintf(xbuf, fmt, ## args);	      \
	if (p + xlen >= endp)                         \
		return -1;			      \
	memcpy(p, xbuf, xlen);                        \
	p += xlen;                                    \
} while(0)

#endif /* HAVE_SNPRINTF */

static int hextoint(const u_int8_t *p, unsigned int l)
{
	char buf[16], *e;
	unsigned int r;

	memcpy(buf, p, l);
	buf[l] = 0;
	r = strtoul(buf, &e, 16);
	if (*e)
		return -1;
	return r;
}

int snprintpkt(char *buf, size_t sz, const u_int8_t *pkt, unsigned int len)
{
	static const u_int8_t dp0ais_str[6] = { 'D' << 1, 'P' << 1, '0' << 1, 'A' << 1, 'I' << 1, 'S' << 1 };
	static const u_int8_t data1_str[6] = { 'D' << 1, 'A' << 1, 'T' << 1, 'A' << 1, '1' << 1, ' ' << 1 };
	u_int8_t v1 = 1, cmd = 0, dp0ais = 1, data1 = 1;
	u_int8_t i, j;
	char *p = buf;
	char *endp = buf + sz;
	const u_int8_t *pkt1;
	unsigned int len1;

	if (sz < 2)
		return -1;
	*buf = 0;
	if (!pkt || len < 8)
		return 0;
	if (pkt[1] & 1) {
		/*
		 * FlexNet Header Compression
		 */
		v1 = 0;
		cmd = (pkt[1] & 2) != 0;
		ADDSTR("fm ? to ");
		i = (pkt[2] >> 2) & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		i = ((pkt[2] << 4) | ((pkt[3] >> 4) & 0xf)) & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		i = ((pkt[3] << 2) | ((pkt[4] >> 6) & 3)) & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		i = pkt[4] & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		i = (pkt[5] >> 2) & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		i = ((pkt[5] << 4) | ((pkt[6] >> 4) & 0xf)) & 0x3f;
		if (i) {
			ADDCH(i+0x20);
		}
		ADDF("-%u QSO Nr %u", pkt[6] & 0xf, (pkt[0] << 6) | (pkt[1] >> 2));
		pkt += 7;
		len -= 7;
		dp0ais = 0;
	} else {
		/*
		 * normal header
		 */
		if (len < 15)
			return 0;
		if ((pkt[6] & 0x80) != (pkt[13] & 0x80)) {
			v1 = 0;
			cmd = (pkt[6] & 0x80);
		}
		ADDSTR("fm ");
		for(i = 7; i < 13; i++) {
			if ((pkt[i] &0xfe) != 0x40) {
				ADDCH(pkt[i] >> 1);
			}
			if ((pkt[i] &0xfe) != dp0ais_str[i-7])
				dp0ais = 0;
		}
		ADDF("-%u to ", (pkt[13] >> 1) & 0xf);
		for(i = 0; i < 6; i++) {
			if ((pkt[i] &0xfe) != 0x40) {
				ADDCH(pkt[i] >> 1);
			}
			if ((pkt[i] &0xfe) != data1_str[i])
				data1 = 0;
		}
		ADDF("-%u", (pkt[6] >> 1) & 0xf);
		pkt += 14;
		len -= 14;
		if (!dp0ais) {
			if ((!(pkt[-1] & 1)) && (len >= 7)) {
				ADDSTR(" via ");
			}
			while ((!(pkt[-1] & 1)) && (len >= 7)) {
				for(i = 0; i < 6; i++) 
					if ((pkt[i] &0xfe) != 0x40) {
						ADDCH(pkt[i] >> 1);
					}
				ADDF("-%u", (pkt[6] >> 1) & 0xf);
				pkt += 7;
				len -= 7;
				if ((!(pkt[-1] & 1)) && (len >= 7)) {
					ADDCH(',');
				}
			}
		}
	}
	if (!len) {
		*p = 0;
		return p - buf;
	}
	i = *pkt++;
	len--;
	j = v1 ? ((i & 0x10) ? '!' : ' ') : 
		((i & 0x10) ? (cmd ? '+' : '-') : (cmd ? '^' : 'v'));
	if (!(i & 1)) {
		/*
		 * Info frame
		 */
		ADDF(" I%u%u%c", (i >> 5) & 7, (i >> 1) & 7, j);
	} else if (i & 2) {
		/*
		 * U frame
		 */
		switch (i & (~0x10)) {
		case 0x03:
			ADDF(" UI%c", j);
			break;
		case 0x2f:
			ADDF(" SABM%c", j);
			break;
		case 0x43:
			ADDF(" DISC%c", j);
			break;
		case 0x0f:
			ADDF(" DM%c", j);
			break;
		case 0x63:
			ADDF(" UA%c", j);
			break;
		case 0x87:
			ADDF(" FRMR%c", j);
			break;
		default:
			ADDF(" unknown U (0x%x)%c", i & (~0x10), j);
			break;
		}
	} else {
		/*
		 * supervisory
		 */
		switch (i & 0xf) {
		case 0x1:
			ADDF(" RR%u%c", (i >> 5) & 7, j);
			break;
		case 0x5:
			ADDF(" RNR%u%c", (i >> 5) & 7, j);
			break;
		case 0x9:
			ADDF(" REJ%u%c", (i >> 5) & 7, j);
			break;
		default:
			ADDF(" unknown S (0x%x)%u%c", i & 0xf, (i >> 5) & 7, j);
			break;
		}
	}
	if (!len) {
		ADDCH('\n');
		*p = 0;
		return p - buf;
	}
	ADDF(" pid=%02X\n", *pkt++);
	len--;
	pkt1 = pkt;
	len1 = len;
	j = 0;
	while (len) {
		i = *pkt++;
		if ((i >= 32) && (i < 128)) {
			ADDCH(i);
		}
		else if (i == 13) {
			if (j) {
				ADDCH('\n');
			}
			j = 0;
		} else {
			ADDCH('.');
		}
		if (i >= 32) 
			j = 1;
		len--;
	}
	if (j) {
		ADDCH('\n');
	}
	if (dp0ais && data1 && len1 >= 20) {
		ADDF("Timestamp:    %u\n"
		     "Temp Board:   %5.2f°C\n"
		     "Temp RF PA:   %5.2f°C\n"
		     "U/Battery:    %5.2fV\n"
		     "U/5V:         %5.2fV\n"
		     "U/3.3V:       %5.2fV\n"
		     "Bit Errors:   %u\n",
		     hextoint(pkt1, 4),
		     hextoint(pkt1 + 4, 4) * 0.5,
		     hextoint(pkt1 + 8, 4) * 0.5,
		     hextoint(pkt1 + 12, 2) * 0.1,
		     (hextoint(pkt1 + 14, 2) + 512) * 0.01,
		     (hextoint(pkt1 + 16, 2) + 256) * 0.01,
		     hextoint(pkt1 + 18, 2));
	}
	*p = 0;
	return p - buf;
}

