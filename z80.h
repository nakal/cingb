
/* 

   z80.h

*/

#ifndef Z80_INCLUDED
#define Z80_INCLUDED

#include "globals.h"


#ifdef USE_LITTLE_ENDIAN
typedef union
{
  struct { uint  AF,BC,DE,HL,SP,PC;} W;
  struct { uchar F,A,C,B,E,D,L,H;} B;
} TYPE_Z80_REG;
#else
typedef union
{
  struct { uint  AF,BC,DE,HL,SP,PC;} W;
  struct { uchar A,F,B,C,D,E,H,L;} B;
} TYPE_Z80_REG;
#endif

extern TYPE_Z80_REG Z80_REG;

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

extern int Z80_HALTED;
extern int Z80_IE;
extern ulong Z80_CPUCLKS;
extern uchar Z80_TICKS;
extern int DBLSPEED;
extern unsigned int skipover;
extern int breakpoint,db_trace;

extern int Z80CLKS_JR[2];
extern int Z80CLKS_JP[2];
extern int Z80CLKS_CALL[2];
extern int Z80CLKS_RET[2];
extern int Z80CLKS_PREFIXCB[2];
extern int Z80CLKS_PREFIXCBINDHL[2];
extern int Z80CLKS_RST[2];



extern char errorstr[100];

void StartCPU(void);

#endif

