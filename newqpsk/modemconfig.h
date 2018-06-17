#ifndef _MODEMCONFIG_H
#define _MODEMCONFIG_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CarrierAmpl		(1.0/8.0)

#define WindowLenLog		6
#define WindowLen		(1<<WindowLenLog)		/* 64	*/
#define SymbolLen		(WindowLen/2)			/* 32	*/
#define HalfSymbol		(SymbolLen/2)			/* 16	*/

#define AliasFilterLen		64
#define NumFilters		64

#define RxPipeLen		4

#define SymbolBits		2
#define PhaseLevels		(1<<SymbolBits)			/* 4	*/

#define DataCarriers		15
#define FirstDataCarr		12
#define DataCarrSepar		3

#define TuneCarriers    	((DataCarriers+1)/4)		/* 4	*/
#define FirstTuneCarr		(FirstDataCarr+DataCarrSepar)	/* 15	*/
#define TuneCarrSepar		(DataCarrSepar*4)		/* 12	*/

#define RxTuneTimeout		56

#define RxAverFollow		4

#define RxUpdateHold		16
#define DCDTuneAverWeight	(1.0/16.0)
#define RxDataSyncFollow	5
#define DCDThreshold		(0.3*M_PI*M_PI/PhaseLevels/PhaseLevels)
#define DCDMaxDrop		12
#define	RxFreqFollowWeight	(1.0/32.0)
#define	RxSyncCorrThres		8

#define TxMinIdle		50
#define TxPreData		2
#define TxPostData		1
#define TxJamLen		16

#define MaxInlv			16

#define	RxAvoidPTT		1

#endif
