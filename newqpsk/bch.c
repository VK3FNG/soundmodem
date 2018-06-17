/*
 * This program generates and decodes (15,7) BCH codes.
 *
 * Reference:
 *
 * MIL-STD-188-220B Appendix K
 *
 * C.H Brain G4GUO emailto:chbrain@dircon.co.uk
 *
 * A small modification by Tomi Manninen, OH2BNS <tomi.manninen@hut.fi>
 */
#if 0
#include <stdio.h>
#include <stdlib.h>
#endif

#include "bch.h"

static unsigned char G[8]=
{
  0x68,
  0x34,
  0x1A,
  0x0D,
  0x6E,
  0x37,
  0x73,
  0x51
};

#define A1 0x080D
#define A2 0x2203
#define A3 0x5101
#define A4 0x00D1

static unsigned int parity(unsigned int word, int length)
{
  int i,count;

  for(i=0,count=0;i<length;i++)
  {
    if(word&(1<<i))count++;
  }
  return count&1;
}
unsigned char decode_bch_codeword(unsigned int code, unsigned int *error)
{
  int i,bit;
  int a1,a2,a3,a4;
  unsigned int input, output;

  input = code;
  for(i=0,output=0; i<BCH_CODEWORD_SIZE; i++)
  {
    a1 = parity(A1&code,BCH_CODEWORD_SIZE);
    a2 = parity(A2&code,BCH_CODEWORD_SIZE);
    a3 = parity(A3&code,BCH_CODEWORD_SIZE);
    a4 = parity(A4&code,BCH_CODEWORD_SIZE);
    /* Majority vote */
    if((a1+a2+a3+a4) >= 3)
    {
      bit = (code^1)&1;
    }
    else
    {
      bit = (code^0)&1;
    }
    code   >>=1;
    code   |= bit?0x4000:0;
    output >>=1;
    output |= bit?0x4000:0;
  }
  *error = code ^ input;
  return (unsigned char)(output&0x7F);
}
unsigned int encode_bch_codeword(unsigned char data)
{
  unsigned int word;
  int i;

  for(i=0,word=0; i<8; i++)
  {
    word <<=1;
    word |= parity((data&G[i]),8);
  }
  word = (word<<7)+data;
  return word;
}
/*
 * For test only.
 */
#if 0
int main(int argc, char *argv[])
{
  unsigned int test =0x42;
  unsigned int error=0x1010;

  if(argc >= 3)
  {
      test  = (int)strtol(argv[1],(char**)NULL,16);
      error = (int)strtol(argv[2],(char**)NULL,16);
  }

  printf("INPUT   DATA %.2X\n",test);

  test = encode_bch_codeword(test);

  printf("ENCODED DATA %.4X - %.2X %.2X\n",test, test>>7, test&0x7F);

  printf("ERROR VECTOR %.4X\n",error);

  test ^=error;

  printf("ERROR + DATA %.4X - %.2X %.2X\n",test, test>>7, test&0x7F);

  test = decode_bch_codeword(test, &error);

  printf("DECODED DATA %.2X\n",test);

  printf("ERROR VECTOR %.4X\n",error);

  return test;
}
#endif
/**/

