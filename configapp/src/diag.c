/*****************************************************************************/

/*
 *      diag.c  --  Configuration Application, diagnostic part.
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
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configapp.h"

#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"
#include "callbacks.h"

#include "spectrum.h"
#include "scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ---------------------------------------------------------------------- */

#include "option1.xpm"
#include "option2.xpm"

/* ---------------------------------------------------------------------- */

//#ifdef WIN32
#define TXTERMNOCANCEL
//#endif


#define DIAGFLG_MODEM           1
#define DIAGFLG_SCOPE           2
#define DIAGFLG_SPECTRUM        4
#define DIAGFLG_RECEIVE         8
#define DIAGFLG_P3DRECEIVE      16
#define DIAGFLG_WINDOWMASK      (DIAGFLG_P3DRECEIVE|DIAGFLG_RECEIVE|DIAGFLG_SCOPE|DIAGFLG_SPECTRUM)

static struct {
	unsigned int flags;
	struct modulator *modch;
	struct demodulator *demodch;
	const char *cfgname;
	const char *chname;
	guint timeoutid;
        unsigned int samplerate;
        struct {
                unsigned int newstate;
                unsigned int state;
                unsigned int carrierfreq;
                unsigned int newpacket;
                u_int16_t crc;
                unsigned char packet[512+2];
        } p3d;
 	int dcd, upddcd, dcdfreeze;
	int ptt, updptt, pttthr, updpttthr;
	unsigned int count0, count1, updcount;
	unsigned int rxwr, rxrd;
	unsigned char rxbuf[1024];
	/* HDLC receiver */
        struct {
                unsigned int bitbuf, bitstream, numbits, state, passall;
                unsigned char *bufptr;
                int bufcnt;
                unsigned char buf[RXBUFFER_SIZE];
        } hrx;
        unsigned int traceupd;
        void *modstate, *demodstate;
	unsigned int bitrate;
        pthread_cond_t txcond;
        pthread_mutex_t txmutex;
        pthread_t rxthread, txthread;
	struct pttio pttio;
	struct audioio *audioio;
#ifdef TXTERMNOCANCEL
        unsigned int txterminate;
#endif
} diagstate = {
	0, &modchain_x, &demodchain_x, NULL, NULL, 0,
	0, { 0, 0, 0, 0, }, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0, },
	{ 0, 0, 0, 0, 0, },
        0,
        NULL, NULL, 0,
	PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
#if 0
	/* gcc3 no longer accepts {} as initializer */
        { }, { },
#endif
};

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

static void do_rxpacket(void)
{
        if (diagstate.hrx.bufcnt < 3) 
                return;
#if 1
        if (!diagstate.hrx.passall && !check_crc_ccitt(diagstate.hrx.buf, diagstate.hrx.bufcnt)) 
                return;
#endif
        {
		GtkTextView   *view;
		GtkTextBuffer *model;
                char buf[512];
		int i, len;
                len = snprintpkt(buf, sizeof(buf), diagstate.hrx.buf, diagstate.hrx.bufcnt-2);
		for (i = 0; i < len; i++) {
			if (!g_ascii_isprint(buf[i]) &&
			    !g_ascii_isspace(buf[i]))
				buf[i] = '*';
		}

                g_printerr("Packet: %s\n", buf);
                view  = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(receivewindow), "packettext"));
		model = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
		gtk_text_buffer_insert_at_cursor(model, buf, len);
		gtk_text_view_scroll_mark_onscreen(view, gtk_text_buffer_get_insert(model));
        }
}

#define DECODEITERA(j)                                                        \
do {                                                                          \
        if (!(notbitstream & (0x0fc << j)))              /* flag or abort */  \
                goto flgabrt##j;                                              \
        if ((bitstream & (0x1f8 << j)) == (0xf8 << j))   /* stuffed bit */    \
                goto stuff##j;                                                \
  enditer##j:;                                                                \
} while (0)

#define DECODEITERB(j)                                                                 \
do {                                                                                   \
  flgabrt##j:                                                                          \
        if (!(notbitstream & (0x1fc << j))) {              /* abort received */        \
                state = 0;                                                             \
                goto enditer##j;                                                       \
        }                                                                              \
        if ((bitstream & (0x1fe << j)) != (0x0fc << j))   /* flag received */          \
                goto enditer##j;                                                       \
        if (state)                                                                     \
                do_rxpacket();                                                         \
        diagstate.hrx.bufcnt = 0;                                                      \
        diagstate.hrx.bufptr = diagstate.hrx.buf;                                      \
        state = 1;                                                                     \
        numbits = 7-j;                                                                 \
        goto enditer##j;                                                               \
  stuff##j:                                                                            \
        numbits--;                                                                     \
        bitbuf = (bitbuf & ((~0xff) << j)) | ((bitbuf & ~((~0xff) << j)) << 1);        \
        goto enditer##j;                                                               \
} while (0)
        
static inline void hdlc_receive(const unsigned char *data, unsigned nrbytes)
{
        unsigned bits, bitbuf, notbitstream, bitstream, numbits, state;

        /* start of HDLC decoder */
        numbits = diagstate.hrx.numbits;
        state = diagstate.hrx.state;
        bitstream = diagstate.hrx.bitstream;
        bitbuf = diagstate.hrx.bitbuf;
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
                        if (diagstate.hrx.bufcnt >= RXBUFFER_SIZE) {
                                state = 0;
                        } else {
                                *(diagstate.hrx.bufptr)++ = bitbuf >> (16-numbits);
                                diagstate.hrx.bufcnt++;
                                numbits -= 8;
                        }
                }
        }
        diagstate.hrx.numbits = numbits;
        diagstate.hrx.state = state;
        diagstate.hrx.bitstream = bitstream;
        diagstate.hrx.bitbuf = bitbuf;
}

/* ---------------------------------------------------------------------- */

void p3dreceive(struct modemchannel *chan, const unsigned char *pkt, u_int16_t crc)
{
        memcpy(diagstate.p3d.packet, pkt, 512+2);
        diagstate.p3d.crc = crc;
        diagstate.p3d.newpacket = 1;
}

void p3drxstate(struct modemchannel *chan, unsigned int synced, unsigned int carrierfreq)
{
        diagstate.p3d.state = synced;
        diagstate.p3d.carrierfreq = carrierfreq;
        diagstate.p3d.newstate = 1;
}

/* ---------------------------------------------------------------------- */

static inline unsigned int hweight8(unsigned int w)
{
        unsigned int res = (w & 0x55) + ((w >> 1) & 0x55);
        res = (res & 0x33) + ((res >> 2) & 0x33);
        return (res & 0x0F) + ((res >> 4) & 0x0F);
}

/* ---------------------------------------------------------------------- */

void audiowrite(struct modemchannel *chan, const int16_t *samples, unsigned int nr)
{
        if (!diagstate.audioio->write)
                return;
        diagstate.audioio->write(diagstate.audioio, samples, nr);
}

void audioread(struct modemchannel *chan, int16_t *samples, unsigned int nr, u_int16_t tim)
{
        if (!diagstate.audioio->read) {
                pthread_exit(NULL);
                return;
        }
        diagstate.audioio->read(diagstate.audioio, samples, nr, tim);
}

u_int16_t audiocurtime(struct modemchannel *chan)
{
        if (!diagstate.audioio->curtime)
                return 0;
        return diagstate.audioio->curtime(diagstate.audioio);
}

/* ---------------------------------------------------------------------- */

int pktget(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
	memset(data, 0, len);
#ifdef TXTERMNOCANCEL
        if (diagstate.txterminate)
                return 0;
#endif
	return diagstate.ptt ? len : 0;
}

void pktput(struct modemchannel *chan, const unsigned char *data, unsigned int len)
{
	unsigned int l;

	while (len > 0) {
		l = sizeof(diagstate.rxbuf) - diagstate.rxwr;
		if (l > len)
			l = len;
		memcpy(&diagstate.rxbuf[diagstate.rxwr], data, l);
		data += l;
		diagstate.rxwr = (diagstate.rxwr + l) % sizeof(diagstate.rxbuf);
		len  -= l;
	}
}

void pktsetdcd(struct modemchannel *chan, int dcd)
{
        pttsetdcd(&diagstate.pttio, dcd);
	diagstate.dcd = dcd;
	diagstate.upddcd = 1;
}

static void *transmitter(void *dummy)
{
        diagstate.pttthr = 0;
        diagstate.updpttthr = 1;
        pthread_mutex_lock(&diagstate.txmutex);
        for (;;) {
#ifdef TXTERMNOCANCEL
                if (diagstate.txterminate) {
                        pthread_mutex_unlock(&diagstate.txmutex);
                        return NULL;
                }
#endif
                if (!diagstate.ptt || !diagstate.modch->modulate || !diagstate.audioio->write) {
                        pthread_cond_wait(&diagstate.txcond, &diagstate.txmutex);
                        continue;
                }
                diagstate.pttthr = 1;
                diagstate.updpttthr = 1;
                pttsetptt(&diagstate.pttio, 1);
		if (diagstate.audioio->transmitstart)
                        diagstate.audioio->transmitstart(diagstate.audioio);
                diagstate.modch->modulate(diagstate.modstate, 0);
                if (diagstate.audioio->transmitstop)
                        diagstate.audioio->transmitstop(diagstate.audioio);
                pttsetptt(&diagstate.pttio, 0);
                diagstate.pttthr = 0;
                diagstate.updpttthr = 1;
        }
}

static void *receiver(void *dummy)
{
        if (diagstate.demodch->demodulate)
                diagstate.demodch->demodulate(diagstate.demodstate);
        pktsetdcd(NULL, 0);
        return NULL;
}

/* ---------------------------------------------------------------------- */

static void setled(GtkImage *image, int on)
{
	GdkPixbuf *pixbuf  = gdk_pixbuf_new_from_xpm_data(on ? (const char **)option2_xpm : (const char **)option1_xpm);
	
	gtk_image_set_from_pixbuf(image, pixbuf);
	g_object_unref(pixbuf);
}

GtkWidget* create_led_pixmap(gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)option1_xpm);
	GtkWidget *image  = gtk_image_new_from_pixbuf(pixbuf);

	g_object_unref(pixbuf);
	return image;
}

/* ---------------------------------------------------------------------- */

static inline void update_display_p3d(void)
{
	GtkTextView   *view;
	GtkTextBuffer *model;
        GtkWidget *w;
        unsigned int i, j;
        char buf[4096], *cp;

        if (diagstate.p3d.newstate) {
                diagstate.p3d.newstate = 0;
                snprintf(buf, sizeof(buf), "%s", diagstate.p3d.state == 2 ? "RECEIVE (SYNCSCAN)" : diagstate.p3d.state ? "RECEIVE" : "SYNC HUNT");
                w = GTK_WIDGET(g_object_get_data(G_OBJECT(p3dwindow), "rxstatus"));
                gtk_entry_set_text(GTK_ENTRY(w), buf);
                snprintf(buf, sizeof(buf), "%u", diagstate.p3d.carrierfreq);
                w = GTK_WIDGET(g_object_get_data(G_OBJECT(p3dwindow), "carrierfreq"));
                gtk_entry_set_text(GTK_ENTRY(w), buf);
        }
        if (diagstate.p3d.newpacket) {
                diagstate.p3d.newpacket = 0;
                cp = buf;
                cp += sprintf(cp, "P3D Telemetry Packet: CRC 0x%04x", diagstate.p3d.crc);
                for (i = 0; i < 512; i += 0x10) {
                        cp += sprintf(cp, "\n%04x:", i);
                        for (j = 0; j < 0x10; j++)
                                cp += sprintf(cp, " %02x", diagstate.p3d.packet[i+j]);
                        cp += sprintf(cp, "  ");
                        for (j = 0; j < 0x10; j++) {
                                if (diagstate.p3d.packet[i+j] < ' ' || diagstate.p3d.packet[i+j] >= 0x80)
                                        cp += sprintf(cp, ".");
                                else
                                        cp += sprintf(cp, "%c", diagstate.p3d.packet[i+j]);
                        }
                }
                cp += sprintf(cp, "\n%04x: %02x %02x%44s", 512, diagstate.p3d.packet[512], diagstate.p3d.packet[514], " ");
                for (i = 512; i < 514; i++) {
                        if (diagstate.p3d.packet[i] < ' ' || diagstate.p3d.packet[i] >= 0x80)
                                cp += sprintf(cp, ".");
                        else
                                cp += sprintf(cp, "%c", diagstate.p3d.packet[i]);
                }
                cp += sprintf(cp, "\n\n");
                view  = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(receivewindow), "packettext"));
		model = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
		gtk_text_buffer_insert_at_cursor(model, buf, cp-buf);
		i = gtk_text_buffer_get_char_count(model);
		if (i > 16384) {
			GtkTextIter start, end;
			gchar *text;

			gtk_text_buffer_get_iter_at_offset(model, &start, 16384);
			gtk_text_buffer_get_iter_at_offset(model, &end, -1);
			text = gtk_text_buffer_get_text(model, &start, &end, TRUE);
			gtk_text_buffer_set_text(model, text, -1);
			g_free(text);
		}
		gtk_text_view_scroll_mark_onscreen(view, gtk_text_buffer_get_insert(model));
                /* decode cooked packet if CRC ok or passall selected */
                if (!diagstate.p3d.crc ||
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(p3dwindow), "buttonpassall")))) {
                        
                }
        }
}

static void update_display(void)
{
        GtkWidget *w;
	gboolean b;
	unsigned int c0, c1;
	char buf[64];
	unsigned char ch;

        if (diagstate.upddcd) {
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(receivewindow), "leddcd")), diagstate.dcd);
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(scopewindow), "leddcd")), diagstate.dcd);
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(specwindow), "leddcd")), diagstate.dcd);
		diagstate.upddcd = 0;
	}
	if (diagstate.updptt) {
                b = diagstate.ptt ? TRUE : FALSE;
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(receivewindow), "ptt"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), b);
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(scopewindow), "ptt"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), b);
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(specwindow), "ptt"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), b);
		diagstate.updptt = 0;
	}
        if (diagstate.updpttthr) {
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(receivewindow), "ledptt")), diagstate.pttthr);
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(scopewindow), "ledptt")), diagstate.pttthr);
                setled(GTK_IMAGE(g_object_get_data(G_OBJECT(specwindow), "ledptt")), diagstate.pttthr);
                diagstate.updpttthr = 0;
        }
	if (diagstate.rxrd != diagstate.rxwr) {
		GtkTextView   *view;
		GtkTextBuffer *model;

		view = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(receivewindow), "bitstext"));
		model = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
		while (diagstate.rxrd != diagstate.rxwr) {
			ch = diagstate.rxbuf[diagstate.rxrd];
			diagstate.rxrd = (diagstate.rxrd + 1) % sizeof(diagstate.rxbuf);
			hdlc_receive(&ch, 1);
			c0 = hweight8(ch);
			diagstate.count0 += 8-c0;
			diagstate.count1 += c0;
			buf[0] = '0'+(ch & 1);
			buf[1] = '0'+((ch >> 1) & 1);
			buf[2] = '0'+((ch >> 2) & 1);
			buf[3] = '0'+((ch >> 3) & 1);
			buf[4] = '0'+((ch >> 4) & 1);
			buf[5] = '0'+((ch >> 5) & 1);
			buf[6] = '0'+((ch >> 6) & 1);
			buf[7] = '0'+((ch >> 7) & 1);
			gtk_text_buffer_insert_at_cursor(model, buf, 8);
		}
		c0 = gtk_text_buffer_get_char_count(model);
		if (c0 > 2048) {
			GtkTextIter start, end;
			gchar *text;

			gtk_text_buffer_get_iter_at_offset(model, &start, 2048);
			gtk_text_buffer_get_iter_at_offset(model, &end, -1);
			text = gtk_text_buffer_get_text(model, &start, &end, TRUE);
			gtk_text_buffer_set_text(model, text, -1);
			g_free(text);
		}
		gtk_text_view_scroll_mark_onscreen(view, gtk_text_buffer_get_insert(model));
		diagstate.updcount = 1;
	}
	if (diagstate.updcount) {
		c0 = diagstate.count0;
		c1 = diagstate.count1;
		diagstate.updcount = 0;
		sprintf(buf, "%d", c0);
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(receivewindow), "count0"));
		gtk_entry_set_text(GTK_ENTRY(w), buf);
		sprintf(buf, "%d", c1);
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(receivewindow), "count1"));
		gtk_entry_set_text(GTK_ENTRY(w), buf);
		sprintf(buf, "%d", c0+c1);
		w = GTK_WIDGET(g_object_get_data(G_OBJECT(receivewindow), "counttot"));
		gtk_entry_set_text(GTK_ENTRY(w), buf);
	}
	update_display_p3d();
}

static gint periodictasks(gpointer user_data)
{
        int16_t samp[SPECTRUM_NUMSAMPLES];

        update_display();        
        diagstate.traceupd++;
        if (diagstate.traceupd > 1)
                diagstate.traceupd = 0;
        if (diagstate.traceupd || (diagstate.dcdfreeze && !diagstate.dcd) || !(diagstate.flags & (DIAGFLG_SCOPE|DIAGFLG_SPECTRUM)) ||
	    !diagstate.audioio->read || !diagstate.audioio->curtime)
                return TRUE; /* repeat */
        diagstate.audioio->read(diagstate.audioio, samp, SPECTRUM_NUMSAMPLES, diagstate.audioio->curtime(diagstate.audioio)-SPECTRUM_NUMSAMPLES);
	if (diagstate.flags & DIAGFLG_SCOPE) {
                Scope *scope = SCOPE(g_object_get_data(G_OBJECT(scopewindow), "scope"));
                scope_setdata(scope, &samp[SPECTRUM_NUMSAMPLES-SCOPE_NUMSAMPLES]);
        }
	if (diagstate.flags & DIAGFLG_SPECTRUM) {
                Spectrum *spec = SPECTRUM(g_object_get_data(G_OBJECT(specwindow), "spec"));
                spectrum_setdata(spec, samp);
	}
	return TRUE; /* repeat */
}

/* ---------------------------------------------------------------------- */

void diag_stop(void)
{
        if (!(diagstate.flags & DIAGFLG_MODEM)) {
                diagstate.flags = 0;
                return;
        }
        g_source_remove(diagstate.timeoutid);
#ifdef TXTERMNOCANCEL
        diagstate.txterminate = 1;
        pthread_cond_broadcast(&diagstate.txcond);
#else
        pthread_cancel(diagstate.txthread);
#endif
#if 0
        pthread_cancel(diagstate.rxthread);
#else
	if (diagstate.audioio->terminateread)
		diagstate.audioio->terminateread(diagstate.audioio);
#endif
        g_printerr("Joining TxThread\n");
        pthread_join(diagstate.txthread, NULL);
        g_printerr("Joining RxThread\n");
        pthread_join(diagstate.rxthread, NULL);
        g_printerr("Releasing IO\n");
        diagstate.audioio->release(diagstate.audioio);
        if (diagstate.modch->free)
                diagstate.modch->free(diagstate.modstate);
        if (diagstate.demodch->free)
                diagstate.demodch->free(diagstate.demodstate);
        pttsetptt(&diagstate.pttio, 0);
        pttrelease(&diagstate.pttio);
        diagstate.flags = 0;
        diagstate.ptt = 0;
        diagstate.pttthr = 0;
        diagstate.updptt = 1;
        diagstate.updpttthr = 1;
}

#define MAX_PAR 10

static void getparam(const char *cfgname, const char *chname, const char *typname, 
		     const struct modemparams *par, const char *parptr[MAX_PAR], char params[MAX_PAR][64])
{
	unsigned int i;

        memset(parptr, 0, sizeof(parptr));
        for (i = 0; i < MAX_PAR && par->name; i++, par++)
                if (xml_getprop(cfgname, chname, typname, par->name, params[i], sizeof(params[i])) > 0)
                        parptr[i] = params[i];
}

static int diag_start(void)
{
	struct modulator *modch = &modchain_x;
	struct demodulator *demodch = &demodchain_x;
        char params[MAX_PAR][64];
        const char *parptr[MAX_PAR];
	unsigned int i;

        if (diagstate.flags & DIAGFLG_MODEM)
                return 0;
        pthread_mutex_init(&diagstate.txmutex, NULL);
        pthread_cond_init(&diagstate.txcond, NULL);
#ifdef TXTERMNOCANCEL
        diagstate.txterminate = 0;
#endif
        /* get current config/channel name */
        diagstate.cfgname = diagstate.chname = NULL;
	diagstate.cfgname = g_object_get_data(G_OBJECT(configmodel), "cfgname");
	diagstate.chname = g_object_get_data(G_OBJECT(configmodel), "chname");
        if (!diagstate.cfgname || !diagstate.chname) {
                g_printerr("diag activate: no channel selected\n");
                return -1;
        }
        /* search current modulator/demodulator */
        if (xml_getprop(diagstate.cfgname, diagstate.chname, "mod", "mode", params[0], sizeof(params[0])) > 0) {
                for (; modch && strcmp(modch->name, params[0]); modch = modch->next);
        }
        if (modch)
                diagstate.modch = modch;
        else
                diagstate.modch = &modchain_x;
        if (xml_getprop(diagstate.cfgname, diagstate.chname, "demod", "mode", params[0], sizeof(params[0])) > 0) {
                for (; demodch && strcmp(demodch->name, params[0]); demodch = demodch->next);
        }
        if (demodch)
                diagstate.demodch = demodch;
        else
                diagstate.demodch = &demodchain_x;
        g_print("Modulator: %s Demodulator: %s\n", diagstate.modch->name, diagstate.demodch->name);
        /* prepare modulator/demodulator and find minimum sampling rate */
        diagstate.samplerate = 5000;
        if (diagstate.modch->params && diagstate.modch->config) {
		getparam(diagstate.cfgname, diagstate.chname, "mod", diagstate.modch->params, parptr, params);
		for (i = 0; i < MAX_PAR && diagstate.modch->params[i].name; i++)
			g_print("Modulator: parameter %s value %s\n",
				diagstate.modch->params[i].name, parptr[i] ? : "(null)");
                i = diagstate.samplerate;
                diagstate.modstate = diagstate.modch->config(NULL, &i, parptr);
                if (i > diagstate.samplerate)
                        diagstate.samplerate = i;
        }
        if (diagstate.demodch->params && diagstate.demodch->config) {
		getparam(diagstate.cfgname, diagstate.chname, "demod", diagstate.demodch->params, parptr, params);
		for (i = 0; i < MAX_PAR && diagstate.demodch->params[i].name; i++)
			g_print("Demodulator: parameter %s value %s\n",
				diagstate.demodch->params[i].name, parptr[i] ? : "(null)");
		i = diagstate.samplerate;
                diagstate.demodstate = diagstate.demodch->config(NULL, &i, parptr);
                if (i > diagstate.samplerate)
                        diagstate.samplerate = i;
        }
        g_print("Minimum sampling rate: %u\n", diagstate.samplerate);
        /* start Audio */
	getparam(diagstate.cfgname, NULL, "audio", ioparam_type, parptr, params);
	g_print("Audio IO: type %s\n", parptr[0] ? : "(null)");
	if (parptr[0] && !strcmp(parptr[0], ioparam_type[0].u.c.combostr[1])) {
                getparam(diagstate.cfgname, NULL, "audio", ioparams_filein, parptr, params);
                diagstate.audioio = ioopen_filein(&diagstate.samplerate, IO_RDONLY, parptr);
        } else if (parptr[0] && !strcmp(parptr[0], ioparam_type[0].u.c.combostr[2])) {
                getparam(diagstate.cfgname, NULL, "audio", ioparams_sim, parptr, params);
                diagstate.audioio = ioopen_sim(&diagstate.samplerate, IO_RDWR, parptr);
#ifdef HAVE_ALSA
	} else if (parptr[0] && !strcmp(parptr[0], ioparam_type[0].u.c.combostr[3])) {
                getparam(diagstate.cfgname, NULL, "audio", ioparams_alsasoundcard, parptr, params);
		diagstate.audioio = ioopen_alsasoundcard(&diagstate.samplerate, diagstate.modch->modulate ? IO_RDWR : IO_RDONLY, parptr);
#endif /* HAVE_ALSA */
        } else {
                getparam(diagstate.cfgname, NULL, "audio", ioparams_soundcard, parptr, params);
                diagstate.audioio = ioopen_soundcard(&diagstate.samplerate, diagstate.modch->modulate ? IO_RDWR : IO_RDONLY, parptr);
        }
        if (!diagstate.audioio) {
                if (diagstate.modch->free)
                        diagstate.modch->free(diagstate.modstate);
                if (diagstate.demodch->free)
                        diagstate.demodch->free(diagstate.demodstate);
                error_dialog("Cannot start audio IO\n");
                return -1;
        }
        /* start modems */
        g_print("Real sampling rate: %u\n", diagstate.samplerate);
        if (diagstate.modch->init)
                diagstate.modch->init(diagstate.modstate, diagstate.samplerate);
        if (diagstate.demodch->init)
                diagstate.demodch->init(diagstate.demodstate, diagstate.samplerate, &diagstate.bitrate);
        /* start PTT */
	getparam(diagstate.cfgname, NULL, "ptt", pttparams, parptr, params);
        if (pttinit(&diagstate.pttio, parptr))
                g_printerr("cannot start PTT output\n");
        /* periodic start */
        diagstate.timeoutid = g_timeout_add(100, periodictasks, NULL);
        diagstate.flags |= DIAGFLG_MODEM;
        if (pthread_create(&diagstate.rxthread, NULL, receiver, NULL))
                logerr(MLOG_FATAL, "pthread_create");
        if (pthread_create(&diagstate.txthread, NULL, transmitter, NULL))
                logerr(MLOG_FATAL, "pthread_create");        
        return 0;
}

/* ---------------------------------------------------------------------- */

void on_diagscope_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (diag_start())
                return;
        diagstate.flags |= DIAGFLG_SCOPE;
	gtk_widget_show(scopewindow);
}

void on_diagspectrum_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (diag_start())
		return;
        diagstate.flags |= DIAGFLG_SPECTRUM;
	gtk_widget_show(specwindow);
}

void on_diagmodem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (diag_start())
		return;
        diagstate.flags |= DIAGFLG_RECEIVE;
	gtk_widget_show(receivewindow);
}

void on_diagp3dmodem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (diag_start())
		return;
        diagstate.flags |= DIAGFLG_P3DRECEIVE;
	gtk_widget_show(p3dwindow);
}

void on_diagpassall_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkCheckMenuItem *citem = GTK_CHECK_MENU_ITEM(menuitem);
	printf("passall: %u\n", gtk_check_menu_item_get_active(citem));
	diagstate.hrx.passall = gtk_check_menu_item_get_active(citem);
}

/* ---------------------------------------------------------------------- */

void on_ptt_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
        diagstate.ptt = gtk_toggle_button_get_active(togglebutton) ? 1 : 0;
	diagstate.updptt = 1;
        pthread_cond_broadcast(&diagstate.txcond);
}

void on_clearbutton_clicked(GtkButton *button, gpointer user_data)
{
	diagstate.count0 = diagstate.count1 = 0;
	diagstate.updcount = 1;
        update_display();        
}

gboolean on_spec_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
        Spectrum *spec;
        GtkEntry *entry;
        char buf[16];

        snprintf(buf, sizeof(buf), "%d Hz", (int)(event->x * diagstate.samplerate * (1.0 / SPECTRUM_NUMSAMPLES)));
        entry = GTK_ENTRY(g_object_get_data(G_OBJECT(specwindow), "specfreqpointer"));
        gtk_entry_set_text(entry, buf);
        spec = SPECTRUM(g_object_get_data(G_OBJECT(specwindow), "spec"));
        spectrum_setmarker(spec, event->x);
	return FALSE;
}

gboolean on_specwindow_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        diagstate.flags &= ~DIAGFLG_SPECTRUM;
	gtk_widget_hide(specwindow);
        if (!(diagstate.flags & DIAGFLG_WINDOWMASK))
                diag_stop();
	return TRUE;
}

gboolean on_scopewindow_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        diagstate.flags &= ~DIAGFLG_SCOPE;
	gtk_widget_hide(scopewindow);
        if (!(diagstate.flags & DIAGFLG_WINDOWMASK))
                diag_stop();
	return TRUE;
}

void on_dcdfreeze_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
        diagstate.dcdfreeze = gtk_toggle_button_get_active(togglebutton);
}

gboolean on_receivewindow_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        diagstate.flags &= ~DIAGFLG_RECEIVE;
	gtk_widget_hide(receivewindow);
        if (!(diagstate.flags & DIAGFLG_WINDOWMASK))
                diag_stop();
	return TRUE;
}

gboolean on_p3dwindow_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        diagstate.flags &= ~DIAGFLG_P3DRECEIVE;
	gtk_widget_hide(p3dwindow);
        if (!(diagstate.flags & DIAGFLG_WINDOWMASK))
                diag_stop();
	return TRUE;
}
