
/* 

   z80opc.h

   This file contains the full collection of
   opcodes, which will be directly included
   into z80.c.

*/

/*     -----  Z80 macros -----       */
/* these are the real gb-z80-opcodes */

#ifndef Z80OPC_INCLUDED
#define Z80OPC_INCLUDED

/*
  This might help to save memory on some machines.
 */
#ifndef Z80OPC_GAMEBOYC_SKIP

#define M_INCB(reg) { Z80_REG.B.F&=FLAG_C; \
                    Z80_REG.B.F|=++reg == 0 ? FLAG_Z : 0; \
                    Z80_REG.B.F|=(reg & 0x0F) == 0 ? FLAG_H :0; }

#define M_DECB(reg) { Z80_REG.B.F&=FLAG_C; \
                    Z80_REG.B.F|=--reg == 0 ? FLAG_Z|FLAG_N : FLAG_N; \
                    Z80_REG.B.F|=(reg & 0x0F) == 0x0F ? 0 : FLAG_H; }

#define M_RLCA       { Z80_REG.B.F=(Z80_REG.B.A & 0x80) >> 3; \
		     Z80_REG.B.A=(Z80_REG.B.A<<1)|(Z80_REG.B.A>>7); }

#define M_RRCA       { Z80_REG.B.F=(Z80_REG.B.A & 0x01) << 4; \
		     Z80_REG.B.A=(Z80_REG.B.A>>1)|(Z80_REG.B.A<<7); }
                    
#define M_RLA        { Z80_REG.B.F|=Z80_REG.B.A>>7; \
                     Z80_REG.B.A<<=1; \
                     Z80_REG.B.A|=(Z80_REG.B.F & FLAG_C)>>4; \
                     Z80_REG.B.F=(Z80_REG.B.F & 0x01)<<4; }

#define M_RRA        { Z80_REG.B.F|=Z80_REG.B.A & 0x01; \
                     Z80_REG.B.A>>=1; \
                     Z80_REG.B.A|=(Z80_REG.B.F & FLAG_C)<<3; \
                     Z80_REG.B.F=(Z80_REG.B.F & 0x01)<<4; }

#define M_ADDHL(wreg) { Z80_REG.B.F&=FLAG_Z|FLAG_N|FLAG_H; \
                      Z80_REG.B.F|=(((ulong)Z80_REG.W.HL+(ulong)wreg) \
                                  &0x10000)>>12; \
                      Z80_REG.W.HL=Z80_REG.W.HL+wreg; }
                      
#define M_JR          { Z80_REG.W.PC+=1+(signed char)GetByteAt(Z80_REG.W.PC);\
                      Z80_TICKS+=Z80CLKS_JR[DBLSPEED]; }

#define M_DAA         { if ((Z80_REG.B.F & FLAG_N)==0) { \
                        if (((Z80_REG.B.A & 0x0F)>9)|| \
			    ((Z80_REG.B.F & FLAG_H)>0)) { \
			  Z80_REG.B.A+=6; \
			  Z80_REG.B.F|=(Z80_REG.B.A & 0xF0)==0 ? \
			    FLAG_C|FLAG_H : FLAG_H; \
			 } \
                        if (((Z80_REG.B.A & 0xF0)>0x90)|| \
			    ((Z80_REG.B.F & FLAG_C)>0)) { \
			  Z80_REG.B.A+=0x60; \
			  Z80_REG.B.F|=FLAG_C; \
			 } \
	                } else { \
			   if (((Z80_REG.B.A & 0x0F)>9)|| \
			       ((Z80_REG.B.F & FLAG_H)>0)) { \
			     Z80_REG.B.A-=6; \
			     Z80_REG.B.F|=(Z80_REG.B.A & 0xF0)==0xF0 ? \
			       FLAG_C|FLAG_H : FLAG_H; \
			 } \
			   if (((Z80_REG.B.A & 0xF0)>0x90)|| \
			       ((Z80_REG.B.F & FLAG_C)>0)) { \
			     Z80_REG.B.A-=0x60; \
			     Z80_REG.B.F|=FLAG_C; \
			   } \
			 } \
                         Z80_REG.B.F= Z80_REG.B.A==0 ? Z80_REG.B.F | FLAG_Z : \
                                           Z80_REG.B.F & (0xFF-FLAG_Z); \
                      }

#define M_ADD(reg)    { Z80_REG.B.F=((((Z80_REG.B.A & 0x0F)+(reg & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((uint)Z80_REG.B.A+ \
                                     (uint)reg)& 0x100) >> 4)| \
                                    ((Z80_REG.B.A=(Z80_REG.B.A+reg))==0 ? \
                                     FLAG_Z : 0); \
                      }

#define M_ADC(reg)    { DUMMY_F=(Z80_REG.B.F & FLAG_C)>0 ? 1 :0; \
                        Z80_REG.B.F=((((Z80_REG.B.A & 0x0F)+(reg & 0x0F) \
				     +DUMMY_F) & 0x10) << 1)| \
                                    ((((uint)Z80_REG.B.A+ \
                                     (uint)reg+(uint)DUMMY_F)&0x100)>> 4)| \
                                    ((Z80_REG.B.A=(Z80_REG.B.A+reg+DUMMY_F)) \
				     ==0 ? FLAG_Z : 0); \
                      }

#define M_SUB(reg)    { Z80_REG.B.F=((((Z80_REG.B.A & 0x0F)-(reg & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((uint)Z80_REG.B.A- \
                                     (uint)reg)& 0x100) >> 4)| \
                                    ((Z80_REG.B.A=(Z80_REG.B.A-reg))==0 ? \
                                     FLAG_Z|FLAG_N : FLAG_N); }

#define M_SBC(reg)    { DUMMY_F=(Z80_REG.B.F & FLAG_C)>0 ? 1 :0; \
                        Z80_REG.B.F=((((Z80_REG.B.A & 0x0F)-(reg & 0x0F) \
				     -DUMMY_F) & 0x10) << 1)| \
                                    ((((uint)Z80_REG.B.A- \
                                     (uint)reg-(uint)DUMMY_F)&0x100)>> 4)| \
                                    ((Z80_REG.B.A=(Z80_REG.B.A-reg-DUMMY_F)) \
				     ==0 ? FLAG_Z|FLAG_N : FLAG_N); }

#define M_AND(reg)    { Z80_REG.B.F=((Z80_REG.B.A=(Z80_REG.B.A & reg))==0) ? \
                        FLAG_H | FLAG_Z : FLAG_H; }

#define M_XOR(reg)    { Z80_REG.B.F=((Z80_REG.B.A=(Z80_REG.B.A ^ reg))==0) ? \
                        FLAG_Z : 0; }

#define M_OR(reg)     { Z80_REG.B.F=((Z80_REG.B.A=(Z80_REG.B.A | reg))==0) ? \
                        FLAG_Z : 0; }

#define M_CP(reg)     { Z80_REG.B.F=((((Z80_REG.B.A & 0x0F)-(reg & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((uint)Z80_REG.B.A- \
                                     (uint)reg)& 0x100) >> 4)| \
                                    (Z80_REG.B.A==reg ?  \
				     FLAG_Z|FLAG_N : FLAG_N); }

#define M_RET        { Z80_REG.W.PC=GetWordAt(Z80_REG.W.SP); \
                       Z80_REG.W.SP+=2; \
                       Z80_TICKS+=Z80CLKS_RET[DBLSPEED]; }

#define M_POPW(wreg) { wreg=GetWordAt(Z80_REG.W.SP); \
                       Z80_REG.W.SP+=2; }

#define M_JP         { Z80_REG.W.PC=GetWordAt(Z80_REG.W.PC); \
                       Z80_TICKS+=Z80CLKS_JP[DBLSPEED]; }

#define M_CALL       { Z80_REG.W.SP-=2; \
                       SetWordAt(Z80_REG.W.SP,Z80_REG.W.PC+2); \
                       Z80_REG.W.PC=GetWordAt(Z80_REG.W.PC); \
                       Z80_TICKS+=Z80CLKS_CALL[DBLSPEED]; }

#define M_PUSHW(wreg) { Z80_REG.W.SP-=2; \
                        SetWordAt(Z80_REG.W.SP,wreg); }

#endif /* Z80OPC_GAMEBOYC_SKIP */

#define M_RST(addr) { Z80_REG.W.SP-=2; \
                      SetWordAt(Z80_REG.W.SP,Z80_REG.W.PC); \
                      Z80_REG.W.PC=addr; }

#ifndef Z80OPC_GAMEBOYC_SKIP

#define M_ADDSPN    { DUMMY_F=GetByteAt(Z80_REG.W.PC++); \
                      if ((DUMMY_F & 0x80)==0) { \
                      Z80_REG.B.F=((((Z80_REG.W.SP & 0x0F)+(DUMMY_F & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((ulong)Z80_REG.W.SP+ \
                                     (ulong)DUMMY_F)& 0x10000) >> 12); \
                      Z80_REG.W.SP=Z80_REG.W.SP+DUMMY_F; \
        	      } else { \
                      Z80_REG.B.F=((((Z80_REG.W.SP & 0x0F)- \
                                   ((signed char)DUMMY_F & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((ulong)Z80_REG.W.SP+ \
                                     (signed char)DUMMY_F) \
                                    & 0x10000) >> 12); \
                      Z80_REG.W.SP=Z80_REG.W.SP+(signed char)DUMMY_F; \
		      } \
                    }

#define M_LDHLSPN   { DUMMY_F=GetByteAt(Z80_REG.W.PC++); \
                      if ((DUMMY_F & 0x80)==0) { \
                      Z80_REG.B.F=((((Z80_REG.W.SP & 0x0F)+(DUMMY_F & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((Z80_REG.W.SP & 0xFF)+ \
                                     (uint)DUMMY_F)& 0x100) >> 4); \
                      Z80_REG.W.HL=Z80_REG.W.SP+DUMMY_F; \
        	      } else { \
                      Z80_REG.B.F=((((Z80_REG.W.SP & 0x0F)- \
                                   ((signed char)DUMMY_F & 0x0F)) \
                                    & 0x10) << 1)| \
                                    ((((Z80_REG.W.SP & 0xFF)+ \
                                     (signed char)DUMMY_F) \
                                    & 0x100) >> 4); \
                      Z80_REG.W.HL=Z80_REG.W.SP+(signed char)DUMMY_F; \
		      } \
                    }


/* ----- Z80 macros with CB prefix -----
   NOTE: RLCA != RLC A etc. !!!
   DON'T USE DUMMY_F HERE !!              */

#define M_CBRLC(reg) { Z80_REG.B.F=(reg & 0x80) >> 3; \
                     Z80_REG.B.F|=(reg=((reg << 1)|(reg >> 7)))==0 \
                                  ? FLAG_Z : 0; }

#define M_CBRRC(reg) { Z80_REG.B.F=(reg & 0x01) << 4; \
                     Z80_REG.B.F|=(reg=((reg >> 1)|(reg << 7)))==0 \
                                  ? FLAG_Z : 0; }

#define M_CBRL(reg) { Z80_REG.B.F|=reg >> 7; \
                      reg<<=1;reg|=(Z80_REG.B.F & FLAG_C)>>4; \
                      Z80_REG.B.F=(reg==0 ? FLAG_Z : 0)| \
                                  (Z80_REG.B.F & 0x01)<<4; }
                      
#define M_CBRR(reg) { Z80_REG.B.F|=reg & 0x01; \
                      reg>>=1;reg|=(Z80_REG.B.F & FLAG_C)<<3; \
                      Z80_REG.B.F=(reg==0 ? FLAG_Z : 0)| \
                                  (Z80_REG.B.F & 0x01)<<4; }
                      
#define M_CBSLA(reg) { Z80_REG.B.F=(reg & 0x80) >> 3; \
                       Z80_REG.B.F|=(reg=(reg << 1))==0 \
                                    ? FLAG_Z : 0; }

#define M_CBSRA(reg) { Z80_REG.B.F=(reg & 0x01) << 4; \
                       Z80_REG.B.F|=(reg=((reg >> 1)|(reg & 0x80)))==0 \
                                    ? FLAG_Z : 0; }

#define M_CBSWAP(reg)  { Z80_REG.B.F=(reg=((reg >> 4)|(reg << 4)))==0 ? \
                                   FLAG_Z : 0; \
                       }

#define M_CBSRL(reg) { Z80_REG.B.F=(reg & 0x01) << 4; \
                       Z80_REG.B.F|=(reg=(reg >> 1))==0 \
                                    ? FLAG_Z : 0; }

#define M_CBBIT(n,reg) { Z80_REG.B.F=(Z80_REG.B.F & FLAG_C) | FLAG_H | \
                                     ((reg >> n)&1 ? 0 : FLAG_Z); }

#define M_CBRES(n,reg) { reg&=(1 << n)^0xFF; }
#define M_CBSET(n,reg) { reg|=1 << n; }

#define M_EXECCB(cmd,reg) \
                          { \
                            switch (cmd) { \
			    case 0x00:M_CBRLC(reg); break; \
			    case 0x01:M_CBRRC(reg); break; \
			    case 0x02:M_CBRL(reg);  break; \
			    case 0x03:M_CBRR(reg);  break; \
			    case 0x04:M_CBSLA(reg); break; \
			    case 0x05:M_CBSRA(reg); break; \
			    case 0x06:M_CBSWAP(reg);break; \
			    case 0x07:M_CBSRL(reg); break; \
                            case 0x08:                     \
                            case 0x09:                     \
                            case 0x0A:                     \
                            case 0x0B:                     \
                            case 0x0C:                     \
                            case 0x0D:                     \
                            case 0x0E:                     \
                            case 0x0F:                     \
                              M_CBBIT((cmd & 7),reg);break;\
                            case 0x10:                     \
                            case 0x11:                     \
                            case 0x12:                     \
                            case 0x13:                     \
                            case 0x14:                     \
                            case 0x15:                     \
                            case 0x16:                     \
                            case 0x17:                     \
                              M_CBRES((cmd & 7),reg);break;  \
                            case 0x18:                     \
                            case 0x19:                     \
                            case 0x1A:                     \
                            case 0x1B:                     \
                            case 0x1C:                     \
                            case 0x1D:                     \
                            case 0x1E:                     \
                            case 0x1F:                     \
                              M_CBSET((cmd & 7),reg);break;  \
			    } \
			  }

#define M_PREFIXCB(opc)                                \
        { \
	  switch (opc & 0x07) {                        \
          case 0:M_EXECCB((opc >> 3),Z80_REG.B.B);break; \
          case 1:M_EXECCB((opc >> 3),Z80_REG.B.C);break; \
          case 2:M_EXECCB((opc >> 3),Z80_REG.B.D);break; \
          case 3:M_EXECCB((opc >> 3),Z80_REG.B.E);break; \
          case 4:M_EXECCB((opc >> 3),Z80_REG.B.H);break; \
          case 5:M_EXECCB((opc >> 3),Z80_REG.B.L);break; \
          case 6:                                        \
	    DUMMY_F=GetByteAt(Z80_REG.W.HL);             \
	    M_EXECCB((opc >> 3),DUMMY_F);                \
            if ((opc>>6)!=1)                             \
	      SetByteAt(Z80_REG.W.HL,DUMMY_F);           \
            Z80_TICKS+=Z80CLKS_PREFIXCBINDHL[DBLSPEED];  \
	    break;                                       \
          case 7:M_EXECCB((opc >> 3),Z80_REG.B.A);break; \
          }                                              \
          Z80_TICKS+=Z80CLKS_PREFIXCB[DBLSPEED];         \
	}

#endif /* Z80OPC_GAMEBOYC_SKIP */

#endif /* Z80OPC_INCLUDED */
