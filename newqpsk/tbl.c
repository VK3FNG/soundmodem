#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>

#include "modemconfig.h"

float CosTable[WindowLen];
float SinTable[WindowLen];

float ToneWindowInp[WindowLen];
float ToneWindowOut[WindowLen];
float DataWindowOut[WindowLen];
float DataWindowInp[WindowLen];

float DataIniVectI[DataCarriers];
float DataIniVectQ[DataCarriers];
float TuneIniVectI[TuneCarriers];
float TuneIniVectQ[TuneCarriers];

static void init_sincos(void)
{
	int i;

	for (i = 0; i < WindowLen; i++)
		CosTable[i] = cos(2 * M_PI * i / WindowLen);

	for (i = 0; i < WindowLen; i++)
		SinTable[i] = sin(2 * M_PI * i / WindowLen);
}

#define Bins	(WindowLen/2)

static void init_window(void)
{
	int i, j;
	float t, win, k[9], sum;

	for (i = 0; i < 2 * Bins; i++) {
		t = i - Bins + 0.5;
		win = 0.5 + cos(M_PI * t / Bins)
		    + 0.589 * cos(M_PI * 2 * t / Bins)
		    + 0.100 * cos(M_PI * 3 * t / Bins);

		ToneWindowInp[i] = win / 2.189;		/*/(WindowLen/2); */
	}

	for (i = 0; i < 2 * Bins; i++) {
		t = i - Bins + 0.5;
		win = cos(M_PI / 2 * t / Bins);

		ToneWindowOut[i] = win * win / 1.2;
	}

	k[0] =  0.37353458;
	k[1] =  0.56663815;
	k[2] =  0.19436159;
	k[3] = -0.06777465;
	k[4] = -0.09404610;
	k[5] = -0.01705343;
	k[6] =  0.02536151;
	k[7] =  0.01323943;
	k[8] = -0.00416208;

	for (i = 0; i < 2 * Bins; i++) {
		t = i - Bins + 0.5;
		sum = k[0];
		for (j = 1; j < 9; j++)
			sum += k[j] * cos(M_PI * j * t / Bins);

		DataWindowOut[i] = sum;
	}

	for (i = 0; i < 2 * Bins; i++) {
		t = i - Bins + 0.5;
		sum = k[0];
		for (j = 1; j < 9; j++)
			sum += k[j] * cos(M_PI * j * t / Bins);

		DataWindowInp[i] = sum / 1.7;	/* /(WindowLen/2); */
	}

}

static void init_inivect(void)
{
	int i, j;
	float phi;

	for (i = 1, j = 0; i <= DataCarriers; i++, j++) {
		phi = 8 * M_PI * i * i / DataCarriers;
		DataIniVectI[j] = CarrierAmpl * cos(phi);
		DataIniVectQ[j] = CarrierAmpl * sin(phi);
	}

	for (i = 2, j = 0; i <= DataCarriers; i += 4, j++) {
		phi = 8 * M_PI * i * i / DataCarriers;
		TuneIniVectI[j] = 2 * CarrierAmpl * cos(phi);
		TuneIniVectQ[j] = 2 * CarrierAmpl * sin(phi);
	}
}

void init_tbl(void)
{
	init_sincos();
	init_window();
	init_inivect();
}

#ifdef STANDALONE

static void print_data(float *x, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		printf("\t% .10f", x[i]);
		if ((i + 1) == len)
			printf("\n};\n\n");
		else if ((i + 1) % 4 == 0)
			printf(",\n");
		else
			printf(",");
	}
}

static void dump_tables(void)
{
	printf("#include \"modemconfig.h\"\n");
	printf("#include \"tbl.h\"\n\n");

	printf("float ToneWindowInp[WindowLen] = {\n");
	print_data(ToneWindowInp, WindowLen);

	printf("float ToneWindowOut[WindowLen] = {\n");
	print_data(ToneWindowOut, WindowLen);

	printf("float DataWindowOut[WindowLen] = {\n");
	print_data(DataWindowOut, WindowLen);

	printf("float DataWindowInp[WindowLen] = {\n");
	print_data(DataWindowInp, WindowLen);

	printf("float DataIniVectI[DataCarriers] = {\n");
	print_data(DataIniVectI, DataCarriers);
	printf("float DataIniVectQ[DataCarriers] = {\n");
	print_data(DataIniVectQ, DataCarriers);

	printf("float TuneIniVectI[TuneCarriers] = {\n");
	print_data(TuneIniVectI, TuneCarriers);
	printf("float TuneIniVectQ[TuneCarriers] = {\n");
	print_data(TuneIniVectQ, TuneCarriers);

	printf("float CosTable[WindowLen] = {\n");
	print_data(CosTable, WindowLen);

	printf("float SinTable[WindowLen] = {\n");
	print_data(SinTable, WindowLen);
}

int main(int argc, char **argv)
{
	init_tbl();
	dump_tables();
	return 0;
}

#endif
