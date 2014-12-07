/* 
   actionreplay.c

   With this file, it's possible to enter
   action-replay codes.
*/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "globals.h"
#include "arplay.h"

#define MAX_ARCODES 8
#define AR_CODELENGTH 8

Ar_Code arcode[MAX_ARCODES];
int codesentered=0;
int ar_enabled=0;

void ar_setcode(char *code)
{
  unsigned int n,lcode=0,hcode=0;

  if (codesentered>=MAX_ARCODES) {
    fprintf(OUTSTREAM,"Too many action replay codes.\n");
    return;
  }
  if (strlen(code)==AR_CODELENGTH) {
    for (n=0;n<AR_CODELENGTH;n++) {
      code[n]=toupper(code[n]);
      if ((code[n]>='0' && code[n]<='9')||
	  (code[n]>='A' && code[n]<='F')); else {
	    fprintf(OUTSTREAM,"Action replay code faulty (%s).\n",code);
	    return;
	  }
    }
  } else {
    fprintf(OUTSTREAM,"Action replay code faulty (%s).\n",code);
    return;
  }
  
  printf("Got: Code: %s=>",code);
  sscanf(code,"%04X%04X",&hcode,&lcode);
  lcode=(lcode>>8)|((lcode&0xFF)<<8);
  
  arcode[codesentered].addr=lcode;
  arcode[codesentered].test=hcode>>8;
  arcode[codesentered].set=hcode&0xFF;

  printf("%04X; %02X=>%02X\n",arcode[codesentered].addr,
	 arcode[codesentered].test,arcode[codesentered].set);
  
  codesentered++;
}

int ar_checkwrite(unsigned int addr,unsigned char value)
{
  int n;

  if (ar_enabled) {
    for (n=0;n<codesentered;n++)
      if (arcode[n].addr==addr) {
	/*      if (arcode[n].test==arcode[n].set)*/
	return arcode[n].set;
	/*	else
		return arcode[n].test==value ? arcode[n].set : value;*/
      }
  }
  return value;
}
