
/*

  debug.c

  This file supplies cingb with useful debug-information.

*/


#include <stdio.h>

#include "globals.h"
#include "debug.h"
#include "z80.h"
#include "gameboy.h"

const char GB_SREG[][5]={"B","C","D","E","H","L","(HL)","A"};
const char GB_DREG[][3]={"BC","DE","HL","SP"};
const char GB_DPP[][3]={"BC","DE","HL","AF"};
const char GB_FJR[][4]={"","NZ,","Z,","NC,","C,"};
const char GB_FRET[][4]={"NZ","Z","NC","C"};
const char GB_ALU[][4]={"ADD","ADC","SUB","SBC","AND","XOR","OR","CP"};
const char GB_CBOPC[][7]={"RLC","RRC","RL","RR",
                    "SLA","SRA","SWAP","SRL",
                    "BIT 0,","BIT 1,","BIT 2,","BIT 3,", 
                    "BIT 4,","BIT 5,","BIT 6,","BIT 7,",
                    "RES 0,","RES 1,","RES 2,","RES 3,",
                    "RES 4,","RES 5,","RES 6,","RES 7,",
                    "SET 0,","SET 1,","SET 2,","SET 3,",
                    "SET 4,","SET 5,","SET 6,","SET 7,"};


const char debugger_help1[]="\
b <addr>   - set a breakpoint, at address <addr>\n\
c          - clear breakpoint\n\
d <addr>   - dump memory, at address <addr>\n\
e <addr>   - edit ONE byte at address <addr>, write\n\
             value (you'll be asked)\n\
g [<addr>] - go to address <addr>, without <addr> the\n";

const char debugger_help2[]="\
             program will continue from PC\n\
h          - display this help\n\
o <count>  - skip over commands,\n\
             count (decimal number)\n\
q          - quits the debugger and closes everything\n\
v <addr>   - view a value (byte), at address <addr>\n\
#          - graphical lcd-dump\
";

int FlagSet(int);


int FlagSet(flag)
int flag;
{
  return (Z80_REG.B.F & flag) > 0 ? 1 : 0;
}

void DebugState(void)
{
  fprintf(OUTSTREAM,
	  "AF:%04X BC:%04X DE:%04X HL:%04X SP:%04X Z:%i N:%i H:%i C:%i I:%i",
	 Z80_REG.W.AF,Z80_REG.W.BC,Z80_REG.W.DE,Z80_REG.W.HL,Z80_REG.W.SP,
	 FlagSet(FLAG_Z),FlagSet(FLAG_N),FlagSet(FLAG_H),FlagSet(FLAG_C),
	 Z80_IE);
  fprintf(OUTSTREAM," ROM%03X RAM%02X\n",rombanknr,rambanknr);
  fprintf(OUTSTREAM,"CLKS: %06X (%c) ",(unsigned int)Z80_CPUCLKS,
	  DBLSPEED ? 'D' : 'S');
  fprintf(OUTSTREAM,"FF0F: %02X, FF40: %02X, FF41: %02X, ",
	 GetByteAt(0xff0f),
	 GetByteAt(0xff40),
	 GetByteAt(0xff41));
  fprintf(OUTSTREAM,"FF44: %02X, FF45: %02X, FFFF: %02X\n",
	 GetByteAt(0xff44),
	 GetByteAt(0xff45),
	 GetByteAt(0xffff));

  fprintf(OUTSTREAM,"%04X:   %02X %02X %02X  ",Z80_REG.W.PC,
         GetByteAt(Z80_REG.W.PC),
         GetByteAt(Z80_REG.W.PC+1),
         GetByteAt(Z80_REG.W.PC+2)
        );

  switch (GetByteAt(Z80_REG.W.PC))
        {
                case 0x00:fprintf(OUTSTREAM,"NOP");
                          break;
                case 0x01:
                case 0x11:
                case 0x21:
                case 0x31:fprintf(OUTSTREAM,
				  "LD %s,%04X",GB_DREG[GetByteAt(Z80_REG.W.PC) 
				 >> 4],GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0x02:
                case 0x12:fprintf(OUTSTREAM,"LD (%s),A",GB_DREG[GetByteAt(
				 Z80_REG.W.PC)>>4]);
                          break;
                case 0x03:
                case 0x13:
                case 0x23:
                case 0x33:fprintf(OUTSTREAM,
				  "INC %s",GB_DREG[GetByteAt(Z80_REG.W.PC) 
				 >> 4]);
                          break;
                case 0x04:
                case 0x0C:
                case 0x14:
                case 0x1C:
                case 0x24:
                case 0x2C:
                case 0x34:
                case 0x3C:fprintf(OUTSTREAM,
				  "INC %s",GB_SREG[GetByteAt(Z80_REG.W.PC) 
				 >> 3]);
                          break;
                case 0x05:
                case 0x0D:
                case 0x15:
                case 0x1D:
                case 0x25:
                case 0x2D:
                case 0x35:
                case 0x3D:fprintf(OUTSTREAM,
				  "DEC %s",GB_SREG[GetByteAt(Z80_REG.W.PC) 
				  >> 3]);
                          break;
                case 0x06:
                case 0x0E:
                case 0x16:
                case 0x1E:
                case 0x26:
                case 0x2E:
                case 0x36:
                case 0x3E:fprintf(OUTSTREAM,
				  "LD %s,%02X",GB_SREG[GetByteAt(Z80_REG.W.PC)
				  >> 3],
                                  GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0x07:fprintf(OUTSTREAM,"RLCA");
                          break;
                case 0x08:fprintf(OUTSTREAM,"LD (%04X),SP",
                          GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0x09:
                case 0x19:
                case 0x29:
                case 0x39:fprintf(OUTSTREAM,
				  "ADD HL,%s",GB_DREG[GetByteAt(Z80_REG.W.PC)
				  >>4]);
                          break;
                case 0x0A:
                case 0x1A:fprintf(OUTSTREAM,
				  "LD A,(%s)",GB_DREG[GetByteAt(Z80_REG.W.PC)
				  >>4]);
                          break;
                case 0x0B:
                case 0x1B:
                case 0x2B:
                case 0x3B:fprintf(OUTSTREAM,"DEC %s",
				  GB_DREG[GetByteAt(Z80_REG.W.PC)>>4]);
                          break;
                case 0x0F:fprintf(OUTSTREAM,"RRCA");
                          break;
                case 0x10:fprintf(OUTSTREAM,"STOP");
                          break;
                case 0x17:fprintf(OUTSTREAM,"RLA");
                          break;
                case 0x1F:fprintf(OUTSTREAM,"RRA");
                          break;
                case 0x18:
                case 0x20:
                case 0x28:
                case 0x30:
                case 0x38:fprintf(OUTSTREAM,"JR %s%04X",
				  GB_FJR[(GetByteAt(Z80_REG.W.PC)
				 >>3)-3],Z80_REG.W.PC+
				 (signed char)(GetByteAt(Z80_REG.W.PC+1))+2);
                          break;
                case 0x22:fprintf(OUTSTREAM,"LD (HLI),A");
                          break;
                case 0x2A:fprintf(OUTSTREAM,"LD A,(HLI)");
                          break;
                case 0x32:fprintf(OUTSTREAM,"LD (HLD),A");
                          break;
                case 0x3A:fprintf(OUTSTREAM,"LD A,(HLD)");
                          break;
                case 0x27:fprintf(OUTSTREAM,"DAA");
                          break;
                case 0x2F:fprintf(OUTSTREAM,"CPL");
                          break;
                case 0x37:fprintf(OUTSTREAM,"SCF");
                          break;
                case 0x3F:fprintf(OUTSTREAM,"CCF");
                          break;
                case 0x40:
                case 0x41:
                case 0x42:
                case 0x43:
                case 0x44:
                case 0x45:
                case 0x46:
                case 0x47:
                case 0x48:
                case 0x49:
                case 0x4A:
                case 0x4B:
                case 0x4C:
                case 0x4D:
                case 0x4E:
                case 0x4F:
                case 0x50:
                case 0x51:
                case 0x52:
                case 0x53:
                case 0x54:
                case 0x55:
                case 0x56:
                case 0x57:
                case 0x58:
                case 0x59:
                case 0x5A:
                case 0x5B:
                case 0x5C:
                case 0x5D:
                case 0x5E:
                case 0x5F:
                case 0x60:
                case 0x61:
                case 0x62:
                case 0x63:
                case 0x64:
                case 0x65:
                case 0x66:
                case 0x67:
                case 0x68:
                case 0x69:
                case 0x6A:
                case 0x6B:
                case 0x6C:
                case 0x6D:
                case 0x6E:
                case 0x6F:
                case 0x70:
                case 0x71:
                case 0x72:
                case 0x73:
                case 0x74:
                case 0x75:
                case 0x77:
                case 0x78:
                case 0x79:
                case 0x7A:
                case 0x7B:
                case 0x7C:
                case 0x7D:
                case 0x7E:
                case 0x7F:
                          fprintf(OUTSTREAM,"LD %s,%s",
				  GB_SREG[(GetByteAt(Z80_REG.W.PC) >> 3)-8],
                                 GB_SREG[(GetByteAt(Z80_REG.W.PC) & 7)]);
                          break;
                case 0x76:fprintf(OUTSTREAM,"HALT");
                          break;
                case 0x80:
                case 0x81:
                case 0x82:
                case 0x83:
                case 0x84:
                case 0x85:
                case 0x86:
                case 0x87:fprintf(OUTSTREAM,"ADD A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0x88:
                case 0x89:
                case 0x8A:
                case 0x8B:
                case 0x8C:
                case 0x8D:
                case 0x8E:
                case 0x8F:fprintf(OUTSTREAM,"ADC A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0x90:
                case 0x91:
                case 0x92:
                case 0x93:
                case 0x94:
                case 0x95:
                case 0x96:
                case 0x97:fprintf(OUTSTREAM,"SUB A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0x98:
                case 0x99:
                case 0x9A:
                case 0x9B:
                case 0x9C:
                case 0x9D:
                case 0x9E:
                case 0x9F:fprintf(OUTSTREAM,"SBC A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0xA0:
                case 0xA1:
                case 0xA2:
                case 0xA3:
                case 0xA4:
                case 0xA5:
                case 0xA6:
                case 0xA7:fprintf(OUTSTREAM,"AND A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0xA8:
                case 0xA9:
                case 0xAA:
                case 0xAB:
                case 0xAC:
                case 0xAD:
                case 0xAE:
                case 0xAF:fprintf(OUTSTREAM,"XOR A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0xB0:
                case 0xB1:
                case 0xB2:
                case 0xB3:
                case 0xB4:
                case 0xB5:
                case 0xB6:
                case 0xB7:fprintf(OUTSTREAM,"OR A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0xB8:
                case 0xB9:
                case 0xBA:
                case 0xBB:
                case 0xBC:
                case 0xBD:
                case 0xBE:
                case 0xBF:fprintf(OUTSTREAM,"CP A,%s",
				  GB_SREG[GetByteAt(Z80_REG.W.PC)& 7]);
                          break;
                case 0xC0:
                case 0xC8:
                case 0xD0:
                case 0xD8:fprintf(OUTSTREAM,"RET %s",
				  GB_FRET[(GetByteAt(Z80_REG.W.PC)>>3)&3]);
                          break;
                case 0xC1:
                case 0xD1:
                case 0xE1:
                case 0xF1:fprintf(OUTSTREAM,"POP %s",
				  GB_DPP[(GetByteAt(Z80_REG.W.PC)>>4)-0x0C]);
                          break;
                case 0xC2:
                case 0xCA:
                case 0xD2:
                case 0xDA:fprintf(OUTSTREAM,"JP %s,%04X",
				  GB_FRET[(GetByteAt(Z80_REG.W.PC)
				 >>3)&3],GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0xC3:fprintf(OUTSTREAM,"JP %04X",
				  GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0xC4:
                case 0xCC:
                case 0xD4:
                case 0xDC:fprintf(OUTSTREAM,"CALL %s,%04X",
				  GB_FRET[(GetByteAt(Z80_REG.W.PC)>>3)&3],
                                 GetWordAt(Z80_REG.W.PC+1));
		          break;
                case 0xC5:
                case 0xD5:
                case 0xE5:
                case 0xF5:fprintf(OUTSTREAM,"PUSH %s",
				  GB_DPP[(GetByteAt(Z80_REG.W.PC)
				 >>4)-0x0C]);
                          break;
                case 0xC6:
                case 0xCE:
                case 0xD6:
                case 0xDE:
                case 0xE6:
                case 0xEE:
                case 0xF6:
                case 0xFE:fprintf(OUTSTREAM,"%s A,%02X",
				  GB_ALU[(GetByteAt(Z80_REG.W.PC)
				 >>3)&7],GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0xC7:
                case 0xCF:
                case 0xD7:
                case 0xDF:
                case 0xE7:
                case 0xEF:
                case 0xF7:
                case 0xFF:fprintf(OUTSTREAM,"RST %02X",
				  ((GetByteAt(Z80_REG.W.PC)>>3)&7)<<3);
                          break;
                case 0xC9:fprintf(OUTSTREAM,"RET");
                          break;
                case 0xCB:fprintf(OUTSTREAM,"%s %s",
				  GB_CBOPC[GetByteAt(Z80_REG.W.PC+1)
				 >>3],GB_SREG[GetByteAt(Z80_REG.W.PC+1)&7]);
                          break;
                case 0xCD:fprintf(OUTSTREAM,"CALL %04X",
				  GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0xD9:fprintf(OUTSTREAM,"RETI");
                          break;
                case 0xE0:fprintf(OUTSTREAM,"LDH (%02X),A",
				  GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0xE2:fprintf(OUTSTREAM,"LDH (C),A");
                          break;
                case 0xE8:fprintf(OUTSTREAM,"ADD SP,%02X",
				  GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0xE9:fprintf(OUTSTREAM,"JP (HL)");
                          break;
                case 0xEA:fprintf(OUTSTREAM,"LD (%04X),A",
				  GetWordAt(Z80_REG.W.PC+1));
                          break;
                case 0xF0:fprintf(OUTSTREAM,"LDH A,(%02X)",
				  GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0xF2:fprintf(OUTSTREAM,"LDH A,(C)");
                          break;
                case 0xF3:fprintf(OUTSTREAM,"DI");
                          break;
                case 0xFB:fprintf(OUTSTREAM,"EI");
                          break;
                case 0xF8:fprintf(OUTSTREAM,"LD HL,SP+%02X",
				  GetByteAt(Z80_REG.W.PC+1));
                          break;
                case 0xF9:fprintf(OUTSTREAM,"LD SP,HL");
                          break;
                case 0xFA:fprintf(OUTSTREAM,"LD A,(%04X)",
				  GetWordAt(Z80_REG.W.PC+1));
                          break;
	default:fprintf(OUTSTREAM,"*** illegal opcode: 0x%02X ***",
		       GetByteAt(Z80_REG.W.PC));
	  break;
	}
}

