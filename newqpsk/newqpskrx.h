#ifndef _NEWQPSKRX_H
#define _NEWQPSKRX_H

/* --------------------------------------------------------------------- */

struct rxstate {
	struct modemchannel *chan;
	struct fecstate fec;
	struct filter filt;
	unsigned int bps;
	unsigned int shreg;
	unsigned int mintune;
	unsigned int minsync;
	void (*rxroutine) (void *);
	float *rxwindowfunc;
	unsigned int rxphase;

	complex rxbuf[256];
	unsigned bufptr;
	unsigned buflen;

	int skip;

	complex rxpipe[RxPipeLen][DataCarriers];
	unsigned rxptr;

	float srate;			/* internal samplerate		*/

	float carrfreq;			/* current rx carrier frequency	*/
	float carrphase;		/* current rx NCO phase		*/

	complex rxwin[WindowLen];
	complex fftbuf[WindowLen];
	int rxphasecorr;

	int acceptance;
	int atsymbol;
	int statecntr;
	int updhold;
	int bitbatches;

	/* tune mode power and correlations */
	float tunepower[TuneCarriers];
	float tunephase[TuneCarriers];
	complex tunecorr[TuneCarriers];

	/* sync mode at-symbol power and correlations */
	float power_at[TuneCarriers];
	complex corr1_at[TuneCarriers];
	complex corr2_at[TuneCarriers];

	/* sync mode inter-symbol power and correlations */
	float power_inter[TuneCarriers];
	complex corr1_inter[TuneCarriers];
	complex corr2_inter[TuneCarriers];

	float syncphase[TuneCarriers];
	float syncdelay[TuneCarriers];

	/* data mode */
	float phesum[DataCarriers];	/* Phase error sum              */
	float pheavg[DataCarriers];	/* Phase error average          */
	float dcdavg[DataCarriers];	/* Phase error power average    */
	float power[DataCarriers];	/* Carrier power average        */
	float correl[DataCarriers];	/* Sync correlation average     */

	float phemax;			/* maximum phase error sum      */

	int fecerrors[DataCarriers];	/* FEC errors per carrier       */
};

/* --------------------------------------------------------------------- */

extern void init_newqpskrx(void *);
extern void newqpskrx(void *, complex *);

/* --------------------------------------------------------------------- */

#endif
