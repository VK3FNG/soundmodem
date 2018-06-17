#ifndef _NEWQPSKTX_H
#define _NEWQPSKTX_H

#include "complex.h"
#include "modemconfig.h"

/* --------------------------------------------------------------------- */

struct txstate {
	struct modemchannel *chan;
	struct fecstate fec;
	struct filter filt;
	unsigned int bps;
	unsigned int shreg;
	unsigned int bufsize;
	unsigned int tunelen;
	unsigned int synclen;
	void (*txroutine) (void *);
	int statecntr;
	int tuneonly;
	int txdone;
	int empty;
	float *txwindowfunc;
	complex tunevect[TuneCarriers];
	complex datavect[DataCarriers];
	unsigned txword[SymbolBits];
	complex txwin[WindowLen];
	complex fftbuf[WindowLen];
	int saved;
};

/* --------------------------------------------------------------------- */

extern void init_newqpsktx(void *);
extern int newqpsktx(void *, complex *);

/* --------------------------------------------------------------------- */

#endif
