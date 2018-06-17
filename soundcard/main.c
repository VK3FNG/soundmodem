/*****************************************************************************/

/*
 *      main.c  --  Soundmodem main.
 *
 *      Copyright (C) 1999-2001, 2003, 2010
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
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
#include "simd.h"

#include "getopt.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libxml includes */
#include <libxml/tree.h>
#include <libxml/parser.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#include <sys/types.h>
#include <sys/mman.h>

/* ---------------------------------------------------------------------- */

struct state state = {
	NULL, NULL, NULL,
	{ 150, 40, 100, 0, 10 }, {}
};

static struct modemparams chaccparams[] = {
        { "txdelay", "TxDelay", "Transmitter Keyup delay in ms", "150", MODEMPAR_NUMERIC, { n: { 0, 2550, 10, 50 } } },
        { "slottime", "Slot Time", "Slot Time in ms (normally 100ms)", "100", MODEMPAR_NUMERIC, { n: { 0, 2550, 10, 50 } } },
        { "ppersist", "P-Persistence", "P-Persistence", "40", MODEMPAR_NUMERIC, { n: { 0, 255, 1, 10 } } },
        { "fulldup", "Full Duplex", "Full Duplex", "0", MODEMPAR_CHECKBUTTON },
        { "txtail", "TxTail", "Transmitter Tail delay in ms", "10", MODEMPAR_NUMERIC, { n: { 0, 2550, 10, 50 } } },
        { NULL }
};

#ifdef HAVE_ALSA
#define ALSA_STR , "alsa"
#else /* HAVE_ALSA */
#define ALSA_STR 
#endif /* HAVE_ALSA */

static struct modemparams ioparam_type[] = {
	{ "type", "Audio IO Mode", "Audio IO Mode", "soundcard", MODEMPAR_COMBO, 
	  { c: { { "soundcard", "file", "simulation" ALSA_STR } } } },
	{ NULL }
};

#undef ALSA_STR

/* ---------------------------------------------------------------------- */

void audiowrite(struct modemchannel *chan, const int16_t *samples, unsigned int nr)
{
        struct audioio *audioio = chan->state->audioio;

        if (!audioio->write)
                return;
        audioio->write(audioio, samples, nr);
}

void audioread(struct modemchannel *chan, int16_t *samples, unsigned int nr, u_int16_t tim)
{
        struct audioio *audioio = chan->state->audioio;

        if (!audioio->read) {
                pthread_exit(NULL);
                return;
        }
        audioio->read(audioio, samples, nr, tim);
}

u_int16_t audiocurtime(struct modemchannel *chan)
{
        struct audioio *audioio = chan->state->audioio;

        if (!audioio->curtime)
                return 0;
        return audioio->curtime(audioio);
}

/* ---------------------------------------------------------------------- */

#define MAXPAR 16

static void getparam(xmlDocPtr doc, xmlNodePtr node, const struct modemparams *par, const char *parstr[MAXPAR])
{
	unsigned int i;

	memset(parstr, 0, sizeof(parstr));
	if (!par || !node)
		return;
	for (i = 0; i < MAXPAR && par->name; i++, par++)
		parstr[i] = xmlGetProp(node, par->name);
}

static void *demodthread(void *state)
{
	struct modemchannel *chan = state;
	chan->demod->demodulate(chan->demodstate);
	logprintf(MLOG_FATAL, "Receiver %s has returned\n", chan->demod->name);
	return NULL;
}

static void parsechannel(xmlDocPtr doc, xmlNodePtr node, struct state *state, unsigned int *samplerate)
{
	xmlNodePtr pkt = NULL, mod = NULL, demod = NULL;
	struct modulator *mc = NULL;
	struct demodulator *dc = NULL;
	struct modemchannel *chan;
	char *cp;
	const char *par[MAXPAR];
	unsigned int sr, ismkiss = 0;

	for (; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) 
			continue; 
		if (!node->name)
			logprintf(MLOG_FATAL, "Node has no name\n");
		if (!strcmp(node->name, "pkt")) {
			pkt = node;
			ismkiss = 0;
			cp = xmlGetProp(node, "mode");
			if (cp && (!strcmp(cp, "mkiss") || !strcmp(cp, "MKISS")))
				ismkiss = 1;
                        continue;
		}
		if (!strcmp(node->name, "mod")) {
			mod = node;
			continue;
		}
		if (!strcmp(node->name, "demod")) {
			demod = node;
			continue;
		}
		logprintf(MLOG_ERROR, "unknown node \"%s\"\n", node->name);
	}
	if (mod) {
		cp = xmlGetProp(mod, "mode");
		if (cp) {
			for (mc = modchain; mc && strcmp(mc->name, cp); mc = mc->next);
			if (!mc)
				logprintf(MLOG_ERROR, "Modulator \"%s\" unknown\n", cp);
		}
	}
	if (demod) {
		cp = xmlGetProp(demod, "mode");
		if (cp) {
			for (dc = demodchain; dc && strcmp(dc->name, cp); dc = dc->next);
			if (!dc)
				logprintf(MLOG_ERROR, "Demodulator \"%s\" unknown\n", cp);
		}
	}
	if ((!mc && !dc) || !pkt)
		return;
	if (!(chan = malloc(sizeof(struct modemchannel))))
		logprintf(MLOG_FATAL, "out of memory\n");
        memset(chan, 0, sizeof(struct modemchannel));
	chan->next = state->channels;
	chan->state = state;
	chan->mod = mc;
	chan->demod = dc;
	chan->modstate = NULL;
	chan->demodstate = NULL;
	if (ismkiss) {
		getparam(doc, pkt, pktmkissparams, par);
		pktinitmkiss(chan, par);
	} else {
		getparam(doc, pkt, pktkissparams, par);
		pktinit(chan, par);
	}
	if (mc) {
		getparam(doc, mod, mc->params, par);
		sr = *samplerate;
		chan->modstate = mc->config(chan, &sr, par);
		if (sr > *samplerate)
			*samplerate = sr;
	}
	if (dc) {
		getparam(doc, demod, dc->params, par);
		sr = *samplerate;
		chan->demodstate = dc->config(chan, &sr, par);
		if (sr > *samplerate)
			*samplerate = sr;
	}
	state->channels = chan;
}

static int parsecfg(xmlDocPtr doc, xmlNodePtr node, struct state *state, unsigned int *schedrr)
{
	xmlNodePtr audio = NULL;
	xmlNodePtr ptt = NULL;
	const char *par[MAXPAR];
	struct modemchannel *chan;
        pthread_attr_t rxattr;
	size_t stacksize;
	unsigned int samplerate = 5000, mode;

	for (; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) 
			continue; 
		if (!node->name)
			logprintf(MLOG_FATAL, "Node has no name\n");
		if (!strcmp(node->name, "audio")) {
			audio = node;
			continue;
		}
		if (!strcmp(node->name, "ptt")) {
			ptt = node;
			continue;
		}
		if (!strcmp(node->name, "chaccess")) {
			getparam(doc, node, chaccparams, par);
			if (par[0])
				state->chacc.txdelay = strtoul(par[0], NULL, 0);
			if (par[1])
				state->chacc.slottime = strtoul(par[1], NULL, 0);
			if (par[2])
				state->chacc.ppersist = strtoul(par[2], NULL, 0);
			if (par[3])
				state->chacc.fullduplex = !!strtoul(par[3], NULL, 0);
			if (par[4])
				state->chacc.txtail = strtoul(par[4], NULL, 0);
			continue;
		}
		if (!strcmp(node->name, "channel")) {
			if (node->children)
				parsechannel(doc, node->children, state, &samplerate);
			continue;
		}
		logprintf(MLOG_ERROR, "unknown node \"%s\"\n", node->name);
	}
        /* find audio mode */
        mode = 0;
        for (chan = state->channels; chan; chan = chan->next) {
                if (chan->demod && chan->demod->demodulate)
                        mode |= IO_RDONLY;
                if (chan->mod && chan->mod->modulate)
                        mode |= IO_WRONLY;
        }
	if (!state->channels || !mode) {
		logprintf(MLOG_ERROR, "no channels configured\n");
		return -1;
	}
        /* open PTT */
	getparam(doc, ptt, pttparams, par);
	if (pttinit(&state->ptt, par))
                logprintf(MLOG_ERROR, "cannot start PTT output\n");
        /* open audio */
	getparam(doc, audio, ioparam_type, par);
	if (par[0] && !strcmp(par[0], ioparam_type[0].u.c.combostr[1])) {
		getparam(doc, audio, ioparams_filein, par);
		state->audioio = ioopen_filein(&samplerate, IO_RDONLY, par);
		if (schedrr)
			*schedrr = 0;
	} else if (par[0] && !strcmp(par[0], ioparam_type[0].u.c.combostr[2])) {
                getparam(doc, audio, ioparams_sim, par);
		state->audioio = ioopen_sim(&samplerate, IO_RDWR, par);
		if (schedrr)
			*schedrr = 0;
#ifdef HAVE_ALSA
	} else if (par[0] && !strcmp(par[0], ioparam_type[0].u.c.combostr[3])) {
                getparam(doc, audio, ioparams_alsasoundcard, par);
		state->audioio = ioopen_alsasoundcard(&samplerate, mode, par);
#endif /* HAVE_ALSA */
	} else {
		getparam(doc, audio, ioparams_soundcard, par);
		state->audioio = ioopen_soundcard(&samplerate, mode, par);
	}
	if (!state->audioio)
                logprintf(MLOG_FATAL, "cannot start audio\n");
	for (chan = state->channels; chan; chan = chan->next) {
		if (chan->demod) {
			chan->demod->init(chan->demodstate, samplerate, &chan->rxbitrate);
                        if (pthread_attr_init(&rxattr))
				logerr(MLOG_FATAL, "pthread_attr_init");
			/* needed on FreeBSD, according to driehuis@playbeing.org */
			if (pthread_attr_getstacksize(&rxattr, &stacksize))
				logerr(MLOG_ERROR, "pthread_attr_getstacksize");
			else if (stacksize < 256*1024)
				if (pthread_attr_setstacksize(&rxattr, 256*1024))
					logerr(MLOG_ERROR, "pthread_attr_setstacksize");
#ifdef HAVE_SCHED_H
                        if (schedrr && *schedrr) {
                                struct sched_param schp;
                                memset(&schp, 0, sizeof(schp));
                                schp.sched_priority = sched_get_priority_min(SCHED_RR)+1;
                                if (pthread_attr_setschedpolicy(&rxattr, SCHED_RR))
                                        logerr(MLOG_ERROR, "pthread_attr_setschedpolicy");
                                if (pthread_attr_setschedparam(&rxattr, &schp))
                                        logerr(MLOG_ERROR, "pthread_attr_setschedparam");
                        }
#endif /* HAVE_SCHED_H */
			if (pthread_create(&chan->rxthread, &rxattr, demodthread, chan))
				logerr(MLOG_FATAL, "pthread_create");
                        pthread_attr_destroy(&rxattr);
		}
		if (chan->mod)
			chan->mod->init(chan->modstate, samplerate);
	}
	return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef HAVE_MLOCKALL
#define MLOCKOPT "M"
#define MLOCKHLP " [-M]"
#else /* HAVE_MLOCKALL */
#define MLOCKOPT ""
#define MLOCKHLP ""
#endif /* HAVE_MLOCKALL */

static void parseopts(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{ "config", 1, 0, 'c' },
		{ "syslog", 0, 0, 's' },
		{ "nosimd", 0, 0, 'S' },
		{ "daemonize", 0, 0, 'D' },
		{ 0, 0, 0, 0 }
	};
	char *configname = NULL, *cfgname, *filename = "/etc/ax25/soundmodem.conf";
	unsigned int verblevel = 2, tosyslog = 0, simd = 1, schedrr = 0, lockmem = 0, daemonize = 0;
        int c, err = 0;
	xmlDocPtr doc;
	xmlNodePtr node;
        int pfds[2];
	pid_t pid;
	unsigned char uch;	

        while ((c = getopt_long(argc, argv, "v:sSc:RD" MLOCKOPT, long_options, NULL)) != EOF) {
                switch (c) {
                case 'v':
                        verblevel = strtoul(optarg, NULL, 0);
                        break;

		case 's':
			tosyslog = 1;
			break;

		case 'S':
			simd = 0;
			break;

		case 'c':
			configname = optarg;
			break;

                case 'R':
                        schedrr = 1;
                        break;

#ifdef HAVE_MLOCKALL
                case 'M':
                        lockmem = 1;
                        break;
#endif /* HAVE_MLOCKALL */

		case 'D':
			daemonize = 1;
			break;

                default:
                        err++;
                        break;
                }
        }
	if (err) {
                fprintf(stderr, "usage: [-v <verblevel>] [-s] [-S] [-R]" MLOCKHLP " [-c <configname>] <configfile>\n");
                exit(1);
        }
	loginit(verblevel, tosyslog);
	if (daemonize) {
		if (pipe(pfds))
			logerr(MLOG_FATAL, "pipe");
		switch (pid = fork()) {
		case -1:
			logerr(MLOG_FATAL, "fork");

		case 0: /* child process */
			close(pfds[0]);
			setsid(); /* become a process group leader and drop controlling terminal */
			fclose(stdin); /* no more standard in */
			break;
			
		default: /* parent process */
			close(pfds[1]);
			err = read(pfds[0], &uch, sizeof(uch));
			if (err != sizeof(uch))
				logprintf(MLOG_FATAL, "SoundModem init failed\n");
			exit(0);
		}
        }
	initsimd(simd);
#if 0
	if (optind >= argc)
		logprintf(MLOG_FATAL, "no configuration file specified\n");
#endif
        if (optind < argc)
                filename = argv[optind];
	doc = xmlParseFile(filename);
	if (!doc || !doc->children || !doc->children->name)
		logprintf(MLOG_FATAL, "Error parsing config file \"%s\"\n", filename);
	if (strcmp(doc->children->name, "modem"))
		logprintf(MLOG_FATAL, "Config file does not contain modem data\n");
	for (node = doc->children->children; node; node = node->next) {
		if (!node->name || strcmp(node->name, "configuration"))
			continue;
		if (!configname)
			break;
		cfgname = xmlGetProp(node, "name");
		if (cfgname && !strcmp(cfgname, configname))
			break;
	}
	if (!node)
		logprintf(MLOG_FATAL, "Configuartion not found\n");
	if (!node->children)
		logprintf(MLOG_FATAL, "Configuration empty\n");
	err = parsecfg(doc, node->children, &state, &schedrr);
	xmlFreeDoc(doc);
	if (err)
		exit(1);
        /*
         * lock memory down
         */
#ifdef HAVE_MLOCKALL
        if (lockmem) {
                if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) 
                        logerr(MLOG_ERROR, "mlockall");
        }
#endif /* HAVE_MLOCKALL */
#ifdef HAVE_SCHED_H
        if (schedrr) {
                struct sched_param schp;
                memset(&schp, 0, sizeof(schp));
                schp.sched_priority = sched_get_priority_min(SCHED_RR)+1;
                if (sched_setscheduler(0, SCHED_RR, &schp) != 0)
                        logerr(MLOG_ERROR, "sched_setscheduler");
        }
#endif /* HAVE_SCHED_H */
	if (daemonize) {
		uch = 0;
		if (write(pfds[1], &uch, sizeof(uch)) != sizeof(uch))
                        logerr(MLOG_ERROR, "write");
                close(pfds[1]);
	}
}

/* ---------------------------------------------------------------------- */

struct modulator *modchain = &afskmodulator;
struct demodulator *demodchain = &afskdemodulator;

int main(int argc, char *argv[])
{
	afskmodulator.next = &fskmodulator;
	afskdemodulator.next = &fskdemodulator;
        fskmodulator.next = &pammodulator;
        fskdemodulator.next = &fskpspdemodulator;
        fskpspdemodulator.next = &fskeqdemodulator;
        fskeqdemodulator.next = &pamdemodulator;
	pammodulator.next = &pskmodulator;
	pamdemodulator.next = &pskdemodulator;
	pskmodulator.next = &newqpskmodulator;
	pskdemodulator.next = &newqpskdemodulator;
	newqpskdemodulator.next = &p3ddemodulator;
        ioinit_filein();
        ioinit_soundcard();
        ioinit_sim();
       	parseopts(argc, argv);
	pkttransmitloop(&state);
	exit(0);
}
