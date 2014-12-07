
/* 

   z80.c

   This file emulates the Z80 Gameboy (COLOR) CPU.
   It also contains a part of the debugger.

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "globals.h"
#include "z80.h"
#include "gameboy.h"
#include "sgb.h"

#include "sys.h"
#include "debug.h"

TYPE_Z80_REG Z80_REG;

int Z80_HALTED,DBLSPEED;
int Z80_IE;
ulong Z80_CPUCLKS;
uchar Z80_TICKS,DUMMY_F;
unsigned int skipover;
int breakpoint,lastbreakpoint;
int db_trace;


/* Table of clock-ticks for an opcode */
/* (taken from original Z80)          */

unsigned char Z80_CLKS[2][256]=
{{
  4,12, 8, 8, 4, 4, 8, 8, 20, 8, 8, 8, 4, 4, 8, 8,
  4,12, 8, 8, 4, 4, 8, 8,  8, 8, 8, 8, 4, 4, 8, 8,
  8,12, 8, 8, 4, 4, 8, 4,  8, 8, 8, 8, 4, 4, 8, 4,
  8,12, 8, 8,12,12,12, 4,  8, 8, 8, 8, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  8, 8, 8, 8, 8, 8, 4, 8,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  4, 4, 4, 4, 4, 4, 8, 4,  4, 4, 4, 4, 4, 4, 8, 4,
  8,12,12,12,12,16, 8,16,  8, 4,12, 0,12,12, 8,16,
  8,12,12, 0,12,16, 8,16,  8,16,12, 0,12, 0, 8,16,
 12,12, 8, 0, 0,16, 8,16, 16, 4,16, 0, 0, 0, 8,16,
 12,12, 8, 4, 0,16, 8,16, 12, 8,16, 4, 0, 0, 8,16 },
{
  2, 6, 4, 4, 2, 2, 4, 4, 10, 4, 4, 4, 2, 2, 4, 4,
  2, 6, 4, 4, 2, 2, 4, 4,  4, 4, 4, 4, 2, 2, 4, 4,
  4, 6, 4, 4, 2, 2, 4, 2,  4, 4, 4, 4, 2, 2, 4, 2,
  4, 6, 4, 4, 6, 6, 6, 2,  4, 4, 4, 4, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  4, 4, 4, 4, 4, 4, 2, 4,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  2, 2, 2, 2, 2, 2, 4, 2,  2, 2, 2, 2, 2, 2, 4, 2,
  4, 6, 6, 6, 6, 8, 4, 8,  4, 2, 6, 0, 6, 6, 4, 8,
  4, 6, 6, 0, 6, 8, 4, 8,  4, 8, 6, 0, 6, 0, 4, 8,
  6, 6, 4, 0, 0, 8, 4, 8,  8, 2, 8, 0, 0, 0, 4, 8,
  6, 6, 4, 2, 0, 8, 4, 8,  6, 4, 8, 2, 0, 0, 4, 8 
}};
int Z80CLKS_JR[2]={4,2};
int Z80CLKS_RET[2]={12,6};
int Z80CLKS_JP[2]={4,2};
int Z80CLKS_CALL[2]={12,6};
int Z80CLKS_PREFIXCB[2]={8,4};
int Z80CLKS_PREFIXCBINDHL[2]={8,4};
int Z80CLKS_RST[2]={16,8};


/* (OLD table from vgb)*/
/*
uchar Z80_CLKS[2][0x100]=
{{
   4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,
   4,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
   8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
   8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
   8, 8,12,12,12,16, 8,32, 8, 8,12, 0,12,12, 8,32,
   8, 8,12, 0,12,16, 8,32, 8, 8,12, 0,12, 0, 8,32,
  12, 8, 8, 0, 0,16, 8,32,16, 4,16, 0, 0, 0, 8,32, 
  12, 8, 8, 4, 0,16, 8,32,12, 8,16, 4, 0, 0, 8,32 },{
   2, 6, 4, 4, 2, 2, 4, 2,10, 4, 4, 4, 2, 2, 4, 2,
   2, 6, 4, 4, 2, 2, 4, 2, 4, 4, 4, 4, 2, 2, 4, 2,
   4, 6, 4, 4, 2, 2, 4, 2, 4, 4, 4, 4, 2, 2, 4, 2,
   4, 6, 4, 4, 6, 6, 6, 2, 4, 4, 4, 4, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   4, 4, 4, 4, 4, 4, 2, 4, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
   4, 4, 6, 6, 6, 8, 4,16, 4, 4, 6, 0, 6, 6, 4,16,
   4, 4, 6, 0, 6, 8, 4,16, 4, 4, 6, 0, 6, 0, 4,16,
   6, 4, 4, 0, 0, 8, 4,16, 4, 2, 8, 0, 0, 0, 4,16, 
   6, 4, 4, 2, 0, 8, 4,16, 6, 4, 8, 2, 0, 0, 4,16 }
};
int Z80CLKS_JR[2]={4,2};
int Z80CLKS_JP[2]={4,2};
int Z80CLKS_CALL[2]={8,4};
int Z80CLKS_RET[2]={8,4};
int Z80CLKS_PREFIXCB[2]={8,4};
int Z80CLKS_PREFIXCBINDHL[2]={8,4};
*/

char errorstr[100];

#undef Z80OPC_GAMEBOYC_SKIP /* Full INCLUDE */
#include "z80opc.h"

int ExecOpcode(void)
{
  uint LASTPC;
  uchar Opcode;

  LASTPC=Z80_REG.W.PC;
  Opcode=GetByteAt(LASTPC);
  Z80_REG.W.PC++;
  Z80_TICKS=0;

  switch (Opcode)
    {
    case 0x00:break;                                          /* NOP */
    case 0x01:                                                /* LD BC,NN */
      Z80_REG.W.BC=GetWordAt(Z80_REG.W.PC);Z80_REG.W.PC+=2;
      break;
    case 0x02:                                                /* LD (BC),A */
      SetByteAt(Z80_REG.W.BC,Z80_REG.B.A);
      break;
    case 0x03:Z80_REG.W.BC++;break;                           /* INC BC */
    case 0x04:M_INCB(Z80_REG.B.B);break;                      /* INC B */
    case 0x05:M_DECB(Z80_REG.B.B);break;                      /* DEC B */
    case 0x06:                                                /* LD B,N */
      Z80_REG.B.B=GetByteAt(Z80_REG.W.PC++);break;
    case 0x07:M_RLCA;break;                                   /* RLCA */
    case 0x08:                                                /* LD (NN),SP */
      SetWordAt(GetWordAt(Z80_REG.W.PC),Z80_REG.W.SP);
      Z80_REG.W.PC+=2;break;
    case 0x09:M_ADDHL(Z80_REG.W.BC);break;                    /* ADD HL,BC */
    case 0x0A:                                                /* LD A,(BC) */
      Z80_REG.B.A=GetByteAt(Z80_REG.W.BC);break;
    case 0x0B:Z80_REG.W.BC--;break;                           /* DEC BC */
    case 0x0C:M_INCB(Z80_REG.B.C);break;                      /* INC C */
    case 0x0D:M_DECB(Z80_REG.B.C);break;                      /* DEC C */
    case 0x0E:
      Z80_REG.B.C=GetByteAt(Z80_REG.W.PC++);break;            /* LD C,N */
    case 0x0F:M_RRCA;break;                                   /* RRCA */
    case 0x10:                                                /* STOP */
      switch (KEY1&0x81) {
      case 0x01:
	KEY1=0x80;
	DBLSPEED=1;
	break;
      case 0x81:
	KEY1=0x00;
	DBLSPEED=0;
	break;
      default:
	Z80_IE=Z80_HALTED=1;
	break;
      }
      break;
    case 0x11:                                                /* LD DE,NN */
      Z80_REG.W.DE=GetWordAt(Z80_REG.W.PC);
      Z80_REG.W.PC+=2;
      if (Z80_REG.W.DE==0x1B58) {
	if ((GetWordAt(Z80_REG.W.PC)+GetByteAt(Z80_REG.W.PC+2))==0) {
	  Z80_REG.W.DE=0x0001;
	}
      }
      break;
    case 0x12:                                                /* LD (DE),A */
      SetByteAt(Z80_REG.W.DE,Z80_REG.B.A);
      break;
    case 0x13:Z80_REG.W.DE++;break;                           /* INC DE */
    case 0x14:M_INCB(Z80_REG.B.D);break;                      /* INC D */
    case 0x15:M_DECB(Z80_REG.B.D);break;                      /* DEC D */
    case 0x16:                                                /* LD D,N */
      Z80_REG.B.D=GetByteAt(Z80_REG.W.PC++);break;
    case 0x17:M_RLA;break;                                    /* RLA */
    case 0x18:M_JR;break;                                     /* JR N */
    case 0x19:M_ADDHL(Z80_REG.W.DE);break;                    /* ADD HL,DE */
    case 0x1A:                                                /* LD A,(DE) */
      Z80_REG.B.A=GetByteAt(Z80_REG.W.DE);break;
    case 0x1B:Z80_REG.W.DE--;break;                           /* DEC DE */
    case 0x1C:M_INCB(Z80_REG.B.E);break;                      /* INC E */
    case 0x1D:M_DECB(Z80_REG.B.E);break;                      /* DEC E */
    case 0x1E:
      Z80_REG.B.E=GetByteAt(Z80_REG.W.PC++);break;            /* LD E,N */
    case 0x1F:M_RRA;break;                                    /* RRA */
    case 0x20:                                                /* JR NZ,N */
      if ((Z80_REG.B.F & FLAG_Z)==0) M_JR 
      else Z80_REG.W.PC++;
      break;
    case 0x21:                                                /* LD HL,NN */
      Z80_REG.W.HL=GetWordAt(Z80_REG.W.PC);Z80_REG.W.PC+=2;
      break;
    case 0x22:                                                /* LDI (HL),A */
      SetByteAt(Z80_REG.W.HL++,Z80_REG.B.A);
      break;
    case 0x23:Z80_REG.W.HL++;break;                           /* INC HL */
    case 0x24:M_INCB(Z80_REG.B.H);break;                      /* INC H */
    case 0x25:M_DECB(Z80_REG.B.H);break;                      /* DEC H */
    case 0x26:                                                /* LD H,N */
      Z80_REG.B.H=GetByteAt(Z80_REG.W.PC++);break;
    case 0x27:M_DAA;break;                                    /* DAA */
    case 0x28:                                                /* JR Z,N */
      if ((Z80_REG.B.F & FLAG_Z)!=0) M_JR
      else Z80_REG.W.PC++;
      break;
    case 0x29:M_ADDHL(Z80_REG.W.HL);break;                    /* ADD HL,HL */
    case 0x2A:Z80_REG.B.A=GetByteAt(Z80_REG.W.HL++);break;    /* LDI A,(HL) */
    case 0x2B:Z80_REG.W.HL--;break;                           /* DEC HL */
    case 0x2C:M_INCB(Z80_REG.B.L);break;                      /* INC L */
    case 0x2D:M_DECB(Z80_REG.B.L);break;                      /* DEC L */
    case 0x2E:
      Z80_REG.B.L=GetByteAt(Z80_REG.W.PC++);break;            /* LD L,N */
    case 0x2F:                                                /* CPL */
      Z80_REG.B.A^=0xFF;
      Z80_REG.B.F=(Z80_REG.B.F & (FLAG_Z | FLAG_C))
	|FLAG_N|FLAG_H;
      break;
    case 0x30:                                                /* JR NC,N */
      if ((Z80_REG.B.F & FLAG_C)==0) M_JR
      else Z80_REG.W.PC++;
      break;
    case 0x31:                                                /* LD SP,NN */
      Z80_REG.W.SP=GetWordAt(Z80_REG.W.PC);Z80_REG.W.PC+=2;
      break;
    case 0x32:                                                /* LDD (HL),A */
      SetByteAt(Z80_REG.W.HL--,Z80_REG.B.A);
      break;
    case 0x33:Z80_REG.W.SP++;break;                           /* INC SP */
    case 0x34:                                                /* INC (HL) */
      DUMMY_F=GetByteAt(Z80_REG.W.HL);
      M_INCB(DUMMY_F);
      SetByteAt(Z80_REG.W.HL,DUMMY_F);
      break;                        
    case 0x35:                                                /* DEC (HL) */
      DUMMY_F=GetByteAt(Z80_REG.W.HL);
      M_DECB(DUMMY_F);
      SetByteAt(Z80_REG.W.HL,DUMMY_F);
      break;                        
    case 0x36:                                                /* LD (HL),N */
      SetByteAt(Z80_REG.W.HL,GetByteAt(Z80_REG.W.PC++));break;
    case 0x37:                                                /* SCF */
      Z80_REG.B.F=(Z80_REG.B.F & FLAG_Z) | FLAG_C;break;
    case 0x38:                                                /* JR C,N */
      if ((Z80_REG.B.F & FLAG_C)!=0) M_JR
      else Z80_REG.W.PC++;
      break;
    case 0x39:M_ADDHL(Z80_REG.W.SP);break;                    /* ADD HL,SP */
    case 0x3A:Z80_REG.B.A=GetByteAt(Z80_REG.W.HL--);break;    /* LDD A,(HL) */
    case 0x3B:Z80_REG.W.SP--;break;                           /* DEC SP */
    case 0x3C:M_INCB(Z80_REG.B.A);break;                      /* INC A */
    case 0x3D:M_DECB(Z80_REG.B.A);break;                      /* DEC A */
    case 0x3E:
      Z80_REG.B.A=GetByteAt(Z80_REG.W.PC++);break;            /* LD A,N */
    case 0x3F:                                                /* CCF */
      Z80_REG.B.F=(Z80_REG.B.F ^ FLAG_C) & (FLAG_C | FLAG_Z);
      break;
    case 0x40:Z80_REG.B.B=Z80_REG.B.B;break;                  /* LD B,B */
    case 0x41:Z80_REG.B.B=Z80_REG.B.C;break;                  /* LD B,C */
    case 0x42:Z80_REG.B.B=Z80_REG.B.D;break;                  /* LD B,D */
    case 0x43:Z80_REG.B.B=Z80_REG.B.E;break;                  /* LD B,E */
    case 0x44:Z80_REG.B.B=Z80_REG.B.H;break;                  /* LD B,H */
    case 0x45:Z80_REG.B.B=Z80_REG.B.L;break;                  /* LD B,L */
    case 0x46:Z80_REG.B.B=GetByteAt(Z80_REG.W.HL);break;      /* LD B,(HL) */
    case 0x47:Z80_REG.B.B=Z80_REG.B.A;break;                  /* LD B,A */
    case 0x48:Z80_REG.B.C=Z80_REG.B.B;break;                  /* LD C,B */
    case 0x49:Z80_REG.B.C=Z80_REG.B.C;break;                  /* LD C,C */
    case 0x4A:Z80_REG.B.C=Z80_REG.B.D;break;                  /* LD C,D */
    case 0x4B:Z80_REG.B.C=Z80_REG.B.E;break;                  /* LD C,E */
    case 0x4C:Z80_REG.B.C=Z80_REG.B.H;break;                  /* LD C,H */
    case 0x4D:Z80_REG.B.C=Z80_REG.B.L;break;                  /* LD C,L */
    case 0x4E:Z80_REG.B.C=GetByteAt(Z80_REG.W.HL);break;      /* LD C,(HL) */
    case 0x4F:Z80_REG.B.C=Z80_REG.B.A;break;                  /* LD C,A */
    case 0x50:Z80_REG.B.D=Z80_REG.B.B;break;                  /* LD D,B */
    case 0x51:Z80_REG.B.D=Z80_REG.B.C;break;                  /* LD D,C */
    case 0x52:Z80_REG.B.D=Z80_REG.B.D;break;                  /* LD D,D */
    case 0x53:Z80_REG.B.D=Z80_REG.B.E;break;                  /* LD D,E */
    case 0x54:Z80_REG.B.D=Z80_REG.B.H;break;                  /* LD D,H */
    case 0x55:Z80_REG.B.D=Z80_REG.B.L;break;                  /* LD D,L */
    case 0x56:Z80_REG.B.D=GetByteAt(Z80_REG.W.HL);break;      /* LD D,(HL) */
    case 0x57:Z80_REG.B.D=Z80_REG.B.A;break;                  /* LD D,A */
    case 0x58:Z80_REG.B.E=Z80_REG.B.B;break;                  /* LD E,B */
    case 0x59:Z80_REG.B.E=Z80_REG.B.C;break;                  /* LD E,C */
    case 0x5A:Z80_REG.B.E=Z80_REG.B.D;break;                  /* LD E,D */
    case 0x5B:Z80_REG.B.E=Z80_REG.B.E;break;                  /* LD E,E */
    case 0x5C:Z80_REG.B.E=Z80_REG.B.H;break;                  /* LD E,H */
    case 0x5D:Z80_REG.B.E=Z80_REG.B.L;break;                  /* LD E,L */
    case 0x5E:Z80_REG.B.E=GetByteAt(Z80_REG.W.HL);break;      /* LD E,(HL) */
    case 0x5F:Z80_REG.B.E=Z80_REG.B.A;break;                  /* LD E,A */
    case 0x60:Z80_REG.B.H=Z80_REG.B.B;break;                  /* LD H,B */
    case 0x61:Z80_REG.B.H=Z80_REG.B.C;break;                  /* LD H,C */
    case 0x62:Z80_REG.B.H=Z80_REG.B.D;break;                  /* LD H,D */
    case 0x63:Z80_REG.B.H=Z80_REG.B.E;break;                  /* LD H,E */
    case 0x64:Z80_REG.B.H=Z80_REG.B.H;break;                  /* LD H,H */
    case 0x65:Z80_REG.B.H=Z80_REG.B.L;break;                  /* LD H,L */
    case 0x66:Z80_REG.B.H=GetByteAt(Z80_REG.W.HL);break;      /* LD H,(HL) */
    case 0x67:Z80_REG.B.H=Z80_REG.B.A;break;                  /* LD H,A */
    case 0x68:Z80_REG.B.L=Z80_REG.B.B;break;                  /* LD L,B */
    case 0x69:Z80_REG.B.L=Z80_REG.B.C;break;                  /* LD L,C */
    case 0x6A:Z80_REG.B.L=Z80_REG.B.D;break;                  /* LD L,D */
    case 0x6B:Z80_REG.B.L=Z80_REG.B.E;break;                  /* LD L,E */
    case 0x6C:Z80_REG.B.L=Z80_REG.B.H;break;                  /* LD L,H */
    case 0x6D:Z80_REG.B.L=Z80_REG.B.L;break;                  /* LD L,L */
    case 0x6E:Z80_REG.B.L=GetByteAt(Z80_REG.W.HL);break;      /* LD L,(HL) */
    case 0x6F:Z80_REG.B.L=Z80_REG.B.A;break;                  /* LD L,A */
    case 0x70:SetByteAt(Z80_REG.W.HL,Z80_REG.B.B);break;      /* LD (HL),B */
    case 0x71:SetByteAt(Z80_REG.W.HL,Z80_REG.B.C);break;      /* LD (HL),C */
    case 0x72:SetByteAt(Z80_REG.W.HL,Z80_REG.B.D);break;      /* LD (HL),D */
    case 0x73:SetByteAt(Z80_REG.W.HL,Z80_REG.B.E);break;      /* LD (HL),E */
    case 0x74:SetByteAt(Z80_REG.W.HL,Z80_REG.B.H);break;      /* LD (HL),H */
    case 0x75:SetByteAt(Z80_REG.W.HL,Z80_REG.B.L);break;      /* LD (HL),L */
    case 0x76:Z80_IE=Z80_HALTED=1;break;                      /* HALT */
    case 0x77:SetByteAt(Z80_REG.W.HL,Z80_REG.B.A);break;      /* LD (HL),A */
    case 0x78:Z80_REG.B.A=Z80_REG.B.B;break;                  /* LD A,B */
    case 0x79:Z80_REG.B.A=Z80_REG.B.C;break;                  /* LD A,C */
    case 0x7A:Z80_REG.B.A=Z80_REG.B.D;break;                  /* LD A,D */
    case 0x7B:Z80_REG.B.A=Z80_REG.B.E;break;                  /* LD A,E */
    case 0x7C:Z80_REG.B.A=Z80_REG.B.H;break;                  /* LD A,H */
    case 0x7D:Z80_REG.B.A=Z80_REG.B.L;break;                  /* LD A,L */
    case 0x7E:Z80_REG.B.A=GetByteAt(Z80_REG.W.HL);break;      /* LD A,(HL) */
    case 0x7F:Z80_REG.B.A=Z80_REG.B.A;break;                  /* LD A,A */
    case 0x80:M_ADD(Z80_REG.B.B);break;                       /* ADD A,B */
    case 0x81:M_ADD(Z80_REG.B.C);break;                       /* ADD A,C */
    case 0x82:M_ADD(Z80_REG.B.D);break;                       /* ADD A,D */
    case 0x83:M_ADD(Z80_REG.B.E);break;                       /* ADD A,E */
    case 0x84:M_ADD(Z80_REG.B.H);break;                       /* ADD A,H */
    case 0x85:M_ADD(Z80_REG.B.L);break;                       /* ADD A,L */
    case 0x86:M_ADD(GetByteAt(Z80_REG.W.HL));break;           /* ADD A,(HL) */
    case 0x87:M_ADD(Z80_REG.B.A);break;                       /* ADD A,A */
    case 0x88:M_ADC(Z80_REG.B.B);break;                       /* ADC A,B */
    case 0x89:M_ADC(Z80_REG.B.C);break;                       /* ADC A,C */
    case 0x8A:M_ADC(Z80_REG.B.D);break;                       /* ADC A,D */
    case 0x8B:M_ADC(Z80_REG.B.E);break;                       /* ADC A,E */
    case 0x8C:M_ADC(Z80_REG.B.H);break;                       /* ADC A,H */
    case 0x8D:M_ADC(Z80_REG.B.L);break;                       /* ADC A,L */
    case 0x8E:M_ADC(GetByteAt(Z80_REG.W.HL));break;           /* ADC A,(HL) */
    case 0x8F:M_ADC(Z80_REG.B.A);break;                       /* ADC A,A */
    case 0x90:M_SUB(Z80_REG.B.B);break;                       /* SUB A,B */
    case 0x91:M_SUB(Z80_REG.B.C);break;                       /* SUB A,C */
    case 0x92:M_SUB(Z80_REG.B.D);break;                       /* SUB A,D */
    case 0x93:M_SUB(Z80_REG.B.E);break;                       /* SUB A,E */
    case 0x94:M_SUB(Z80_REG.B.H);break;                       /* SUB A,H */
    case 0x95:M_SUB(Z80_REG.B.L);break;                       /* SUB A,L */
    case 0x96:M_SUB(GetByteAt(Z80_REG.W.HL));break;           /* SUB A,(HL) */
    case 0x97:M_SUB(Z80_REG.B.A);break;                       /* SUB A,A */
    case 0x98:M_SBC(Z80_REG.B.B);break;                       /* SBC A,B */
    case 0x99:M_SBC(Z80_REG.B.C);break;                       /* SBC A,C */
    case 0x9A:M_SBC(Z80_REG.B.D);break;                       /* SBC A,D */
    case 0x9B:M_SBC(Z80_REG.B.E);break;                       /* SBC A,E */
    case 0x9C:M_SBC(Z80_REG.B.H);break;                       /* SBC A,H */
    case 0x9D:M_SBC(Z80_REG.B.L);break;                       /* SBC A,L */
    case 0x9E:M_SBC(GetByteAt(Z80_REG.W.HL));break;           /* SBC A,(HL) */
    case 0x9F:M_SBC(Z80_REG.B.A);break;                       /* SBC A,A */
    case 0xA0:M_AND(Z80_REG.B.B);break;                       /* AND A,B */
    case 0xA1:M_AND(Z80_REG.B.C);break;                       /* AND A,C */
    case 0xA2:M_AND(Z80_REG.B.D);break;                       /* AND A,D */
    case 0xA3:M_AND(Z80_REG.B.E);break;                       /* AND A,E */
    case 0xA4:M_AND(Z80_REG.B.H);break;                       /* AND A,H */
    case 0xA5:M_AND(Z80_REG.B.L);break;                       /* AND A,L */
    case 0xA6:M_AND(GetByteAt(Z80_REG.W.HL));break;           /* AND A,(HL) */
    case 0xA7:M_AND(Z80_REG.B.A);break;                       /* AND A,A */
    case 0xA8:M_XOR(Z80_REG.B.B);break;                       /* XOR A,B */
    case 0xA9:M_XOR(Z80_REG.B.C);break;                       /* XOR A,C */
    case 0xAA:M_XOR(Z80_REG.B.D);break;                       /* XOR A,D */
    case 0xAB:M_XOR(Z80_REG.B.E);break;                       /* XOR A,E */
    case 0xAC:M_XOR(Z80_REG.B.H);break;                       /* XOR A,H */
    case 0xAD:M_XOR(Z80_REG.B.L);break;                       /* XOR A,L */
    case 0xAE:M_XOR(GetByteAt(Z80_REG.W.HL));break;           /* XOR A,(HL) */
    case 0xAF:M_XOR(Z80_REG.B.A);break;                       /* XOR A,A */
    case 0xB0:M_OR(Z80_REG.B.B);break;                        /* OR A,B */
    case 0xB1:M_OR(Z80_REG.B.C);break;                        /* OR A,C */
    case 0xB2:M_OR(Z80_REG.B.D);break;                        /* OR A,D */
    case 0xB3:M_OR(Z80_REG.B.E);break;                        /* OR A,E */
    case 0xB4:M_OR(Z80_REG.B.H);break;                        /* OR A,H */
    case 0xB5:M_OR(Z80_REG.B.L);break;                        /* OR A,L */
    case 0xB6:M_OR(GetByteAt(Z80_REG.W.HL));break;            /* OR A,(HL) */
    case 0xB7:M_OR(Z80_REG.B.A);break;                        /* OR A,A */
    case 0xB8:M_CP(Z80_REG.B.B);break;                        /* CP A,B */
    case 0xB9:M_CP(Z80_REG.B.C);break;                        /* CP A,C */
    case 0xBA:M_CP(Z80_REG.B.D);break;                        /* CP A,D */
    case 0xBB:M_CP(Z80_REG.B.E);break;                        /* CP A,E */
    case 0xBC:M_CP(Z80_REG.B.H);break;                        /* CP A,H */
    case 0xBD:M_CP(Z80_REG.B.L);break;                        /* CP A,L */
    case 0xBE:M_CP(GetByteAt(Z80_REG.W.HL));break;            /* CP A,(HL) */
    case 0xBF:M_CP(Z80_REG.B.A);break;                        /* CP A,A */
    case 0xC0:if ((Z80_REG.B.F & FLAG_Z)==0) M_RET;break;     /* RET NZ */
    case 0xC1:M_POPW(Z80_REG.W.BC);break;                     /* POP BC */
    case 0xC2:                                                /* JP NZ,NN */
      if ((Z80_REG.B.F & FLAG_Z)==0) M_JP
      else Z80_REG.W.PC+=2;
      break;
    case 0xC3:M_JP;break;                                     /* JP NN */
    case 0xC4:                                                /* CALL NZ,NN */
      if ((Z80_REG.B.F & FLAG_Z)==0) M_CALL 
      else Z80_REG.W.PC+=2;
      break;
    case 0xC5:M_PUSHW(Z80_REG.W.BC);break;                    /* PUSH BC */
    case 0xC6:                                                /* ADD A,N */
      M_ADD(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;           
    case 0xC7:M_RST(0x00);break;                              /* RST 00 */
    case 0xC8:if ((Z80_REG.B.F & FLAG_Z)!=0) M_RET;break;     /* RET Z */
    case 0xC9:M_RET;break;                                    /* RET */
    case 0xCA:                                                /* JP Z,NN */
      if ((Z80_REG.B.F & FLAG_Z)!=0) M_JP
      else Z80_REG.W.PC+=2;
      break;
    case 0xCB:                                                /* CB-PREFIX */
      M_PREFIXCB(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xCC:                                                /* CALL Z,NN */
      if ((Z80_REG.B.F & FLAG_Z)!=0) M_CALL 
      else Z80_REG.W.PC+=2;
      break;
    case 0xCD:M_CALL;break;                                   /* CALL NN */
    case 0xCE:                                                /* ADC A,N */
      M_ADC(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xCF:M_RST(0x08);break;                              /* RST 08 */
    case 0xD0:if ((Z80_REG.B.F & FLAG_C)==0) M_RET;break;     /* RET NC */
    case 0xD1:M_POPW(Z80_REG.W.DE);break;                     /* POP DE */
    case 0xD2:                                                /* JP NC,NN */
      if ((Z80_REG.B.F & FLAG_C)==0) M_JP 
      else Z80_REG.W.PC+=2;
      break;
 /* case 0xD3: illegal opcode */
    case 0xD4:                                                /* CALL NC,NN */
      if ((Z80_REG.B.F & FLAG_C)==0) M_CALL 
      else Z80_REG.W.PC+=2;
      break;
    case 0xD5:M_PUSHW(Z80_REG.W.DE);break;                    /* PUSH DE */
    case 0xD6:                                                /* SUB A,N */
      M_SUB(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xD7:M_RST(0x10);break;                              /* RST 10 */
    case 0xD8:if ((Z80_REG.B.F & FLAG_C)!=0) M_RET;break;     /* RET C */
    case 0xD9:M_RET;Z80_IE=1;break;                           /* RETI */
    case 0xDA:                                                /* JP C,NN */
      if ((Z80_REG.B.F & FLAG_C)!=0) M_JP
      else Z80_REG.W.PC+=2;
      break;
 /* case 0xDB: illegal opcode */
    case 0xDC:                                                /* CALL C,NN */
      if ((Z80_REG.B.F & FLAG_C)!=0) M_CALL 
      else Z80_REG.W.PC+=2;
      break;
 /* case 0xDD: illegal opcode */
    case 0xDE:                                                /* SUB A,N */
      M_SBC(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xDF:M_RST(0x18);break;                              /* RST 18 */
    case 0xE0:                                              /* LD (FF00+N),A */
      SetByteAt(0xFF00+GetByteAt(Z80_REG.W.PC++),
		Z80_REG.B.A);break;
    case 0xE1:M_POPW(Z80_REG.W.HL);break;                     /* POP HL */
    case 0xE2:                                              /* LD (FF00+C),A */
      SetByteAt(0xFF00+Z80_REG.B.C,Z80_REG.B.A);break;
 /* case 0xE3: illegal opcode */
 /* case 0xE4: illegal opcode */
    case 0xE5:M_PUSHW(Z80_REG.W.HL);break;                    /* PUSH HL */
    case 0xE6:                                                /* ADC A,N */
      M_AND(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xE7:M_RST(0x20);break;                              /* RST 20 */
    case 0xE8:M_ADDSPN;break;                                 /* ADD SP,N */
    case 0xE9:Z80_REG.W.PC=Z80_REG.W.HL;break;                /* JP (HL) */
    case 0xEA:                                                /* LD (NN),A */
      SetByteAt(GetWordAt(Z80_REG.W.PC),Z80_REG.B.A);
      Z80_REG.W.PC+=2;break;
 /* case 0xEB: illegal opcode */
 /* case 0xEC: illegal opcode */
 /* case 0xED: illegal opcode */
    case 0xEE:                                                /* XOR A,N */
      M_XOR(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;           
    case 0xEF:M_RST(0x28);break;                              /* RST 28 */
    case 0xF0:                                              /* LD A,(FF00+N) */
      Z80_REG.B.A=GetByteAt(0xFF00+
		     GetByteAt(Z80_REG.W.PC++));
      break;
    case 0xF1:                                                /* POP AF */
      M_POPW(Z80_REG.W.AF);
      Z80_REG.B.F&=0xF0; /* filter */
      break;
    case 0xF2:                                              /* LD A,(FF00+C) */
      Z80_REG.B.A=GetByteAt(0xFF00+Z80_REG.B.C);
      break;
    case 0xF3:Z80_IE=0;break;                                 /* DI */
    /* case 0xF4: illegal opcode */
    case 0xF5:                                                /* PUSH AF */
      Z80_REG.B.F&=0xF0;
      M_PUSHW(Z80_REG.W.AF);
      break;
    case 0xF6:                                                /* OR A,N */
      M_OR(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xF7:M_RST(0x30);break;                              /* RST 30 */
    case 0xF8:M_LDHLSPN;break;                                /* LD HL,SP+N */
    case 0xF9:Z80_REG.W.SP=Z80_REG.W.HL;break;                /* LD SP,HL */
    case 0xFA:                                                /* LD A,(NN) */
      Z80_REG.B.A=GetByteAt(GetWordAt(Z80_REG.W.PC));
      Z80_REG.W.PC+=2;break;
    case 0xFB:Z80_IE=1;break;                                 /* EI */
 /* case 0xFC: illegal opcode */
 /* case 0xFD: illegal opcode */
    case 0xFE:                                                /* CP A,N */
      M_CP(GetByteAt(Z80_REG.W.PC));
      Z80_REG.W.PC++;
      break;
    case 0xFF:M_RST(0x38);break;                              /* RST 38 */

    default:
      snprintf(errorstr, sizeof(errorstr),"Illegal opcode (%02X) at %X:%X\n",
		      Opcode, rombanknr,LASTPC);
      fputs(errorstr,stderr);
      return 1;
    }
  Z80_TICKS+=Z80_CLKS[DBLSPEED][Opcode];

  return 0;
}

static char *HEX="0123456789ABCDEF";

int getHEX(char *str) {
  int ret;
  unsigned int i,j;

  /*  fprintf(stderr,"analysing '%s'\n",str);*/

  if (str==NULL || strlen(str)>4 || strlen(str)<1) return -1;

  ret=0;
  for (i=0;i<strlen(str);i++) {
    ret<<=4;str[i]=toupper(str[i]);
    for (j=0;j<16;j++) if (str[i]==HEX[j]) break;
    if (j<16) ret+=j; else return -1;
  }

  return ret;
}

void StartCPU()
{
  int err;

#ifdef DEBUG
  int addr,val;
  char cstr[70];
  char cmd;
  char param[70];
  int i;
  int cmdloop;
#endif
  
  Z80_HALTED=0;
  breakpoint=-1;
  lastbreakpoint=-1;skipover=0;db_trace=0;
  err=0;

  while (!err) {
#ifdef DEBUG
    if ((breakpoint==Z80_REG.W.PC)||(skipover==0)) {
      if (breakpoint==Z80_REG.W.PC)
	fprintf(OUTSTREAM,"*Breakpoint reached at 0x%04X\n",breakpoint);
      breakpoint=-1;skipover=0;
      DebugState();fprintf(OUTSTREAM,"\n");
      
      cmdloop=1;
      while (cmdloop) {
	fprintf(OUTSTREAM,":");
	fgets(cstr,sizeof(cstr),stdin);
	if (strlen(cstr)>64) {
	  fprintf(OUTSTREAM,"Command too long.\n");
	  while (getc(stdin)!=10);
	  continue;
	}

	cmd=cstr[0];

	i=1;
	while (cstr[i]==' ') i++;

	if (cstr[i]) strncpy(param,&cstr[i],strlen(&cstr[i])-1);
	param[strlen(&cstr[i])-1]=0;
      
	switch (toupper(cmd)) {
	case 'V':
	  if ((addr=getHEX(param))<0) {
	    fprintf(OUTSTREAM,"invalid address\n");
	    break;
	  }
	  fprintf(OUTSTREAM,"%04X=%02X\n",addr,GetByteAt(addr));
	  break;
	case 'G':
	  if ((addr=getHEX(param))<0) {
	    breakpoint=lastbreakpoint;
	    cmdloop=0;
	    skipover=-1;
	    break;
	  }
	  Z80_REG.W.PC=addr;
	  Z80_HALTED=0;
	  cmdloop=0;
	  skipover=-1;
	  break;
	case 'B':
	  if ((breakpoint=getHEX(param))<0) {
	    fprintf(OUTSTREAM,"Breakpoint is set at: %04X\n",breakpoint);
	    break;
	  }
	  fprintf(OUTSTREAM,"Breakpoint set at: %04X\n",breakpoint);
	  lastbreakpoint=breakpoint;
	  break;
	case 'C':
	  breakpoint=lastbreakpoint=-1;
	  fprintf(OUTSTREAM,"Breakpoint cleared.\n");
	  break;
	case 'D':
	  if ((addr=getHEX(param))<0) {
	    fprintf(OUTSTREAM,"invalid address\n");
	    break;
	  }
	  i=16*16+addr;
	  for (;addr<i;addr+=16) {
	    fprintf(OUTSTREAM,"%04X: %02X %02X %02X %02X %02X %02X %02X %02X \
%02X %02X %02X %02X %02X %02X %02X %02X\n",
	     addr,
	     GetByteAt(addr),GetByteAt(addr+1),
	     GetByteAt(addr+2),GetByteAt(addr+3),
	     GetByteAt(addr+4),GetByteAt(addr+5),
	     GetByteAt(addr+6),GetByteAt(addr+7),
	     GetByteAt(addr+8),GetByteAt(addr+9),
	     GetByteAt(addr+10),GetByteAt(addr+11),
	     GetByteAt(addr+12),GetByteAt(addr+13),
	     GetByteAt(addr+14),GetByteAt(addr+15));
	  }
	  break;
	case 'E':
	  if ((addr=getHEX(param))<0) {
	    fprintf(OUTSTREAM,"invalid address\n");
	    break;
	  }
	  fprintf(OUTSTREAM,"Value (00-FF): ");
	  scanf("%02X",&val);fgetc(stdin);
	  SetByteAt(addr,val);
	  fprintf(OUTSTREAM,"Written 0x%02X => 0x%04X.\n",val,addr);
	  break;
	case 'H':
	  fprintf(OUTSTREAM,"%s%s\n", debugger_help1, debugger_help2);
	  break;
	case 'O':
	  skipover=atoi(param);
	  fprintf(OUTSTREAM,"Skip over commands numer set to: %d.\n",skipover);
	  break;
	case '#':
	  fprintf(OUTSTREAM,"Which screen (0=0x9800,1=9C00) ? ");
	  scanf("%d",&addr);
	  fprintf(OUTSTREAM,"Which tile data (0=0x9000,1=0x8000) ? ");
	  scanf("%d",&i);fgetc(stdin);
	  if (addr!=1) addr=0;
	  if (i!=1) i=0;
#ifdef UNIX
	  vramdump(addr,i);
#endif	
	  break;
	case 'Q':
	  return;
	default:
	  if (!db_trace) {
	    DebugState();fprintf(OUTSTREAM,"\n");
	  }
	  cmdloop=0;
	  skipover=0;
	}
      }
    } else skipover=skipover ? skipover-1 : 0;
#endif
    if (!Z80_HALTED) {
#ifdef DEBUG
      if (db_trace) {DebugState();fprintf(OUTSTREAM,"\n");}
#endif
      err=ExecOpcode();
    }
    
    /* update registers & interrupt processing */
    gameboyspecifics();
  }
  savestate();
}


