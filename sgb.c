
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "sgb.h"

int sgb_packagetransfer=0,sgb_byte=0,sgb_bitnr=0,sgb_bit=0,
  sgb_pkgdone=0,sgb_sgbcheck=0,sgb_mode=0,sgb_pkgnr=0;
unsigned char sgb_buffer[SGB_BUFFERSIZE];

void sgb_dumppackagebuffer(void)
{
  int n;

  fprintf(OUTSTREAM,"SGB-Pkg (CMD=$%02X,#%d): ",SGB_CMD,SGB_PKGCOUNT);
  for (n=0;n<sgb_byte;n++) {
    fprintf(OUTSTREAM,"%02X ",sgb_buffer[n]);
    if (((n&0xF)==0x0F) || (n==(sgb_byte-1)))
      fprintf(OUTSTREAM,"\n");
  }
}

void sgb_processcmd(void)
{
  sgb_pkgnr++;
  if (sgb_pkgnr<SGB_PKGCOUNT) return;

#ifdef DEBUG
		sgb_dumppackagebuffer();
#endif
  sgb_byte=0;sgb_pkgnr=0;
  sgb_mode=1;
  sgb_buffer[0]=0;

  switch (SGB_CMD) {
    /*  case 0:
  case 1:
  case 2:
  case 3:
    if (sgb_byte>15)
      sgbcmd_setpal();
    break;
  case 4:
    if (sgb_byte==2)
    if (sgb_byte>2) {
      if (sgb_byte>1+
    }
    break;*/
  case 0x11:
    /*    if (sgb_byte>15) sgb_sgbcheck=sgb_buffer[1]&1; */
    break;
  }
}

void sgb_closecmd(void)
{
  /*  switch (SGB_CMD) {
  case 0x11:
    sgb_sgbcheck=0;
    break;
    }*/
  sgb_packagetransfer=0;
}

void sgbcmd_setpal(void)
{
  return;
}
