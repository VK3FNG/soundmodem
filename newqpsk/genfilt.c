/*
 * Generate an inline assembler FIR filter optimized for the Pentium.
 * This is inspired by the works of Phil Karn and Thomas Sailer.
 */

#include <stdio.h>

#include "modemconfig.h"

#define asmline(s)		puts("\t\t\"" s ";\\n\\t\"")
#define asmline2(s1,d,s2)	printf("\t\t\"%s%d%s;\\n\\t\"\n", s1, d, s2)

int main(int argc, char **argv)
{
	int i;

	puts("#ifndef _FILTER_I386_H");
	puts("#define _FILTER_I386_H");
	puts("#define __HAVE_ARCH_MAC");

	puts("extern inline float mac(const float *a, const float *b, unsigned int size)");
	puts("{");
	puts("\tfloat f;");
	puts("\tasm volatile (");

	asmline("flds (%1)");
	asmline("fmuls (%2)");
	asmline("flds 4(%1)");
	asmline("fmuls 4(%2)");

	for (i = 2; i < AliasFilterLen; i++) {
		asmline2("flds ", i * 4, "(%1)");
		asmline2("fmuls ", i * 4, "(%2)");
		asmline("fxch %%st(2)");
		asmline("faddp");
	}

	asmline("faddp");

	puts("\t\t: \"=t\" (f) : \"r\" (a) , \"r\" (b) : \"memory\");");
	puts("\treturn f;");
	puts("}");

	puts("#endif");

	return 0;
}
