/*
 * This program generates and decodes (15,7) BCH code words.
 *
 * Reference:
 *
 * MIL-STD-188-220B Appendix K
 *
 * C.H Brain G4GUO emailto:chbrain@dircon.co.uk
 *
 * A small modification by Tomi Manninen, OH2BNS <tomi.manninen@hut.fi>
 */
#ifndef __BCH_H__
#define __BCH_H__

#define BCH_CODEWORD_SIZE 15

unsigned char  decode_bch_codeword(unsigned int  code, unsigned int *err);
unsigned int   encode_bch_codeword(unsigned char data);

#endif



