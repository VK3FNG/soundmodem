#ifndef _TBL_H
#define _TBL_H

extern float AliasFilterInpI[AliasFilterLen];
extern float AliasFilterInpQ[AliasFilterLen];

extern float CosTable[WindowLen];
extern float SinTable[WindowLen];

extern float ToneWindowInp[WindowLen];
extern float ToneWindowOut[WindowLen];
extern float DataWindowOut[WindowLen];
extern float DataWindowInp[WindowLen];

extern float DataIniVectI[DataCarriers];
extern float DataIniVectQ[DataCarriers];
extern float TuneIniVectI[TuneCarriers];
extern float TuneIniVectQ[TuneCarriers];

extern void init_tbl(void);

#endif
