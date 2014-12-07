
/* 

   gameboy.c

   This file emulates the hardware (all around the Gameboy-CPU).

*/


#include <stdio.h>
#include <stdlib.h>

#ifdef UNIX
#include <unistd.h>
#endif

#include <ctype.h>
#include <string.h>

#include "globals.h"
#include "gameboy.h"
#include "sgb.h"
#include "z80.h"

#define Z80OPC_GAMEBOYC_SKIP /* only M_RST needed from here */
#include "z80opc.h"

#include "arplay.h"

#include "sys.h"

#ifdef SOUND
/*#define DEBUG_SOUND*/
#include "sound.h"
#endif

rombank bank[GB_MAXROMBANKCOUNT];      /* rom                           */
rambank sram[GB_MAXRAMBANKCOUNT];      /* sram                          */
uchar *vram[2];                        /* vram                          */
uchar *iram[8];                        /* internal ram                  */
                                       /* don't forget echoing !        */
uchar *oam;                            /* oam                           */
uchar *io;                             /* io                            */


char *lcdbuffer;                       /* gameboy screen buffer         */
unsigned int GB_BGPAL[8][4]=
{{0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000}};
unsigned int GB_OBJPAL[8][4]=
{{0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000},
 {0x7FFF,0x56B5,0x294A,0x0000}};

ulong BGPAL[8][4],OBJPAL[8][4];
 
uint  rombanknr,workbanknr;
uint  romselectmask;
uchar rambanknr;
int   rambankenabled;
uchar sramselectmask;
int   mbctype;
int   mbcmode;
uchar newjoypadstate,joypadstate,lcdphase;
int timerfreq,timerclks,dividerclks,lcdclks,joypadclks,
    vblankdelay,sramsize,srambanksize,lastjoysel,serialioclks,
    soundcounter,ABORT_EMULATION,vblankoccured,
    hdmastarted,hdmasrc,hdmadst,soundclks,snd_updateclks,colormode,
    siosendout;

int serialclks[2][2]={{4096,2048},{128,64}};
                                       /* [SC bit 1][cpuspeed] */

char sramfile[256];

void (*scanline)(void);     /* standard gameboy video output */
void gb_scanline8(void);
void gb_scanline16(void);
void gb_scanline32(void);

void cgb_scanline16(void);  /* gameboy color video output */
void cgb_scanline32(void);
void cgb_scanline8(void);

void switch2color(void);

int GB_SRAMBANKSIZE[]=
{
  0x2000,0x0800,0x2000,0x2000
};
uchar GB_SRAMSELECTMASK[]=
{
  1,1,1,3
};
uchar GB_SRAMBANKCOUNT[]=
{
  1,1,1,4
};

uint GB_VCLKS[]=
{
  80,172,208,560   
};
/*uint GB_VCLKS[]=
{
  80,172,208,460   * cycles 2,3,0,sigma *
};*/
/*uint GB_VCLKS[]=
{
  80,172,204,456   * cycles 2,3,0,sigma (old, I made the refresh slower) *
};*/

uint GB_TIMERFRQ[]=
{
  1024,16,64,256
};

uchar Area_0xFF00_init[]=
{
  0xCF,0x00,0x7E,0xFF,0xAD,0x00,0x00,0xF8,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE1,
  0x80,0xBF,0xF3,0xFF,0x3F,0xFF,0x3F,0x00,
  0xFF,0x3F,0x7F,0xFF,0x9F,0xFF,0x3F,0xFF,
  0xFF,0x00,0x00,0x3F,0x77,0xF3,0xF1,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x91,0x80,0x00,0x00,0x05,0x00,0x00,0xFC,
  0xFC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00
};

uchar Nintendo_Logo[]=
{
  0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,
  0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
  0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,
  0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
  0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,
  0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E
};

uchar H_Flip[]=
{
0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,
0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,
0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,
0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,
0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,
0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,
0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,
0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,
0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,
0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,
0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,
0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,
0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,
0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,
0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,
0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,
0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF,
};

uchar IOIn(uint);
void IOOut(uint,uchar);


uchar IOIn(uint addr)
{
    switch (addr) {
    case 0x44:
	return LY<153 ? LY : 0;

    default:
      return io[addr];
    }
}

uchar GetByteAt(uint addr)
{
#ifdef HARDDEBUG
  if (addr>=0xFF10&&addr<=0xFF27) {
    fprintf(OUTSTREAM,"IO read access: 0x%04X: %04X=>",Z80_REG.W.PC,addr);
    if (addr>=0x4000) {
      if (addr>=0x8000) {
	if (addr>=0xA000) {
	  if (addr>=0xC000) {
	    if (addr>=0xD000) {
	      if (addr>=0xE000) {
		if (addr>=0xF000) {
		  if (addr>=0xFE00) {
		    if (addr>=0xFF00) {
		      fprintf(OUTSTREAM,"%02X",IOIn(addr&0x00FF));
		    } else fprintf(OUTSTREAM,"%02X",oam[addr&0x00FF]);
		  } else fprintf(OUTSTREAM,"%02X",
				 iram[workbanknr][addr&0x0FFF]);
		} else fprintf(OUTSTREAM,"%02X",iram[0][addr&0x0FFF]);
	      } else fprintf(OUTSTREAM,"%02X",
			     iram[workbanknr][addr&0x0FFF]);
	    } else fprintf(OUTSTREAM,"%02X",iram[0][addr&0x0FFF]);
	  } else fprintf(OUTSTREAM,"%02X",sram[rambanknr][addr&0x1FFF]);
	} else fprintf(OUTSTREAM,"%02X",vram[VBK&1][addr&0x1FFF]);
      } else fprintf(OUTSTREAM,"%02X",bank[rombanknr][addr&0x3FFF]);
    } else fprintf(OUTSTREAM,"%02X",bank[0][addr]);
    fprintf(OUTSTREAM,"\n");
  }
#endif
  if (addr<0x4000) return bank[0][addr];
  if (addr<0x8000) return bank[rombanknr][addr & 0x3FFF];
  if (addr<0xA000) return vram[VBK&1][addr & 0x1FFF];
  if (addr<0xC000)
    return /*(addr&0x1FFF)>=srambanksize ? 0 :*/ 
      sram[rambanknr][addr & 0x1FFF];
  if (addr<0xD000) return iram[0][addr&0x0FFF];
  if (addr<0xE000) return iram[workbanknr][addr&0x0FFF];
  if (addr<0xF000) return iram[0][addr&0x0FFF];
  if (addr<0xFE00) return iram[workbanknr][addr&0x0FFF];
  if (addr<0xFF00) return oam[addr & 0x00FF];
  return IOIn(addr & 0x00FF);

  /*  switch (addr >> 13) {
    
      }*/
}

uint GetWordAt(uint addr)
{
  return ((uint)GetByteAt(addr+1)<<8)|GetByteAt(addr);
}

void IOOut(uint addr,uchar value)
{
  int i;
  uint u;

  value=ar_checkwrite(addr+0xFF00,value);
  if (addr<0x4C) {
    switch (addr) {
    case 0x00: /* joypad info */
      if (sgb_packagetransfer && !colormode) {
	switch ((value >> 4)& 0x03) {
	case 0:
#ifdef TDEBUG
	  fprintf(OUTSTREAM,"init\n");
	  sgb_dumppackagebuffer();
#endif
	  sgb_packagetransfer=1;
	  sgb_byte&=0xFFF0;
	  /* sgb_buffer[0]=0;*/
	  sgb_bitnr=0;
	  sgb_bit=-1;
	  break;
	case 1:sgb_bit=1;break;
	case 2:sgb_bit=0;break;
	case 3:
	  if (sgb_bit<0) break;
	  if ((sgb_pkgdone==0)&&(sgb_byte<SGB_BUFFERSIZE)) {
	    sgb_buffer[sgb_byte]|=(sgb_bit<<sgb_bitnr);
	    sgb_bitnr++;
	    if (sgb_bitnr>7) {
	      sgb_bitnr=0;
	      sgb_byte++;
	      sgb_buffer[sgb_byte]=0;
	      
	      /*      printf("(%d,%d)\n",sgb_byte,sgb_bitnr);*/
	      if (!(sgb_byte&0x0F)) {
		sgb_packagetransfer=0;
		sgb_pkgdone=1;
		sgb_processcmd();
	      }
	    }
	  } else {
	      /* package transfer successfully closed */

	      sgb_closecmd();
	  }
	  break;
	}
      }
	
      switch ((value >> 4)& 0x03) {
      case 0:
#ifdef TDEBUG
	fprintf(OUTSTREAM,"init\n");
	sgb_dumppackagebuffer();
#endif
	sgb_packagetransfer=1;
	sgb_pkgdone=0;
	sgb_byte&=0xFFF0;
	sgb_bitnr=0;
	/* sgb_buffer[0]=0;*/
	sgb_bit=-1;
	break;
      case 1:lastjoysel=0;break;
      case 2:lastjoysel=1;break;
      case 3:lastjoysel=2;sgb_packagetransfer=0;break;
      }
      switch (lastjoysel) {
      case 0:
	P1=(joypadstate&0x0F)|0xD0;
	break;
      case 1:
	P1=(joypadstate>>4)|0xE0;
	break;
      case 2:
	/*if (!force_stdgameboy) P1=0x02;*/
	break;
      }
      /*printf("jps: (%02X,%02X,%d)->%02X\n",value,joypadstate,lastjoysel,P1);*/
      break;
    case 0x01: /* serial read/write byte */
      siosendout=value;
      break;
    case 0x02: /* serial control */
      if (colormode)
	serialioclks=serialclks[(SC&2)>>1][DBLSPEED];
      else serialioclks=serialclks[0][0];
#ifdef DIALOGLINK
      if ((value&0x81)==0x81) {
	dlglink_sndbyte(siosendout);
#ifdef HARDDEBUG
	printf("Master sio at PC:%04X (CTRL:%02X) =>%02X.\n",
	       Z80_REG.W.PC,value,siosendout);
#endif
      }
#ifdef HARDDEBUG
      if ((value&0x81)==0x80) {
	printf("Slave sio at PC:%04X (CTRL:%02X) =>%02X.\n",
	       Z80_REG.W.PC,value,siosendout);
      }
#endif
#endif

      SC=value;
#ifndef DIALOGLINK
      if (SC&0x01) IF|=0x08;
      SC&=0x7F;SB=0xFF;
#endif
      break;
    case 0x04: /* reset divider reg */
      DIV=0;dividerclks=GB_DIVLOAD;
      break;
    case 0x07:
      TAC=value|0xF8;
      timerclks=timerfreq=GB_TIMERFRQ[value&0x03];
      break;
    case 0x0F: /* interrupt flag */
      IF=value|0xE0;
      break;
    case 0x40: /* lcd control */
      if ((value & 0x80)==0) {
	LY=0;
	lcdphase=0;
	STAT=(STAT&0xFC)|2;
	lcdclks=GB_VCLKS[0];
      }
      LCDC=value;
      break;
    case 0x41:
      STAT=(STAT&0x07)|0x80|(value&0x78);
      break;
    case 0x44:
      /* should I reset ? */
      break;
    case 0x46:
      for (i=0;i<0xA0;i++)
	/*	if ((i&3)==3)
		oam[i]=(oam[i]&0x0F)|(GetByteAt(((uint)value << 8)+i)&0xF0); 
		else */
	    oam[i]=GetByteAt(((uint)value << 8)+i);
      break;
      
      /* unsupported regs (unchanged) */
    case 0x03:
      
    /* sound control regs */
    case 0x15:
      io[addr]=value;
      break;
    case 0x24: /* handled in sound module */
    case 0x25:
    case 0x26:
      if (addr==0xFF26 && (value&0x80)>0) {
	soundcounter=4000000;
	io[0x26]=0x8F;
      }

#ifdef DEBUG_SOUND
      printf("NR5%d<=%02X\n",(addr-4)&0x7,value);
#endif

      io[addr]=value;
      break;

    case 0x10:
      NR10=value;
#ifdef SOUND
#ifdef DEBUG_SOUND
      if (value&7) printf("NR10<=%02X\n",value);
#endif
      snd1_st=snd1_str=((NR10>>4)&7)<<1;
#endif
      break;
    case 0x11:
      NR11=value;
#ifdef DEBUG_SOUND
      if ((value&0xC0)!=0x80) printf("NR11<=%02X\n",value);
#endif
      break;
    case 0x12:
      NR12=value;
#ifdef DEBUG_SOUND
      printf("NR12<=%02X\n",value);
#endif
#ifdef SOUND
      value=(value&7)<<2;
      if (/*(NR12&8)==0 &&*/ value<snd_length1) snd_length1=value;
#endif
      break;
    case 0x13:
      NR13=value;
#ifdef SOUND
      /*      if (value==0xFF) snd_length1=0;*/
      snd_reload1=gbfreq((((int)(NR14&7)<<8))|NR13);
#endif
#ifdef DEBUG_SOUND
      printf("snd_reload1(low: %05i;%02X-%02X)\n",snd_reload1,NR13,NR12);
#endif
      break;
    case 0x14:
      NR14=value;
#ifdef SOUND
      snd_reload1=gbfreq((((int)(NR14&7)<<8))|NR13);
      if (value&0x80) {
	if ((snd_init1=NR12&0xF7 ? 1 : 0))
	  snd_length1=value & 0x40 ? 65-(NR11&0x3F) : 0xFFFF;
#ifdef DEBUG_SOUND
	printf("%05i %09i (%02X%02X)\n",
	       ((int)(NR14&7)<<8)|NR13,snd_reload1,NR13,NR14);
#endif
      }
#endif
      break;

    case 0x16:
#ifdef DEBUG_SOUND
      if ((value&0xC0)!=0x80) printf("NR21<=%02X\n",value);
#endif
      NR21=value;
      break;
    case 0x17:
      NR22=value;
#ifdef SOUND
      value=(value&7)<<2;
      if (/*(NR22&8)==0 &&*/ value<snd_length2) snd_length2=value;
#endif
      break;
    case 0x18:
      NR23=value;
#ifdef SOUND
      if (value==0xFF) snd_length2=0;
      snd_reload2=gbfreq((((int)(NR24&7)<<8))|NR23);
#endif
      break;
    case 0x19:
      NR24=value;
#ifdef SOUND
      snd_reload2=gbfreq((((int)(NR24&7)<<8))|NR23);
      if (value&0x80) {
	if ((snd_init2=NR22&0xF7 ? 1 : 0))
	  snd_length2=value & 0x40 ? 65-(NR21&0x3F) : 0xFFFF;
      }
#endif
      break;
      
    case 0x1A:
      NR30=value;
      break;
    case 0x1B:
      NR31=value;
      break;
    case 0x1C:
      NR32=value;
#ifdef SOUND
      switch ((value>>5)&3) {
      case 0:snd_shr=4;break;
      case 1:snd_shr=0;break;
      case 2:snd_shr=1;break;
      case 3:snd_shr=2;break;
      }
#endif
      break;
    case 0x1D:
      NR33=value;
#ifdef SOUND
      if (value==0xFF) snd_length3=0;
      snd_reload3=gbfreq((((int)(NR34&7)<<8))|NR33)>>1;
#endif
      break;
    case 0x1E:
      NR34=value;
#ifdef SOUND
      snd_reload3=gbfreq((((int)(NR34&7)<<8))|NR33)>>1;
      if (value&0x80) {
	if ((snd_init3=(NR30>>7)))
	  snd_length3=value & 0x40 ? 257-NR31 : 0xFFFF;
      }
#endif
      break;

    case 0x20:
#ifdef DEBUG_SOUND
      printf("NR41<=%02X\n",value);
#endif
      NR41=value;
      break;
    case 0x21:
#ifdef DEBUG_SOUND
      printf("NR42<=%02X\n",value);
#endif
      NR42=value;
#ifdef SOUND
      value=(value&7)<<2;
      if (/*(NR42&8)==0 &&*/ value<snd_length4) snd_length4=value;
      /*      if ((NR42&0xF7)==0) snd_init4=snd_length4=0;*/
#endif
      break;
    case 0x22:
#ifdef DEBUG_SOUND
      printf("NR43<=%02X\n",value);
#endif
      NR43=value;
      break;
    case 0x23:
#ifdef DEBUG_SOUND
      printf("NR44<=%02X\n",value);
#endif
      NR44=value;
      /*
#ifdef SOUND
      if (value&0x80) {
	snd_reload4=1;
	if ((snd_init4=NR42&0xF8 ? 1 : 0)) {
	  snd_length4=value & 0x40 ? 64-(NR41&0x3F) : 
	    (NR42 & 0x07)<<4;
	  printf("snd_len4 %d\n",snd_length4);
	}
      }
#endif
      */
#ifdef SOUND
      if (value&0x80) {
	snd_reload4=1;
	if ((snd_init4=NR42&0xF7 ? NR44>>7 : 0))
	  snd_length4=value & 0x40 ? 65-(NR41&0x3F) :
	    (NR42 & 0x07)<<4;
      }
#endif
      soundcounter=4000000;
      break;
      

      
      /* unchanged writes */
    case 0x05: /* timer counter          */
    case 0x06: /* timer modulo           */
    case 0x30: /* waveform sound data    */
    case 0x31: /**/
    case 0x32:
    case 0x33:
    case 0x34: /**/
    case 0x35:
    case 0x36:
    case 0x37: /**/
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C: /**/
    case 0x3D:
    case 0x3E:
    case 0x3F: /* --- waveform end       */
    case 0x42: /* scroll y               */
    case 0x43: /* scroll x               */
    case 0x45: /* lcd y compare          */
    case 0x47: /* BG palette             */
    case 0x48: /* sprite palette 0       */
    case 0x49: /* sprite palette 1       */
    case 0x4A: /* window y               */
    case 0x4B: /* window x               */
      io[addr]=value;
      break;
    default:
      return;
    }
  } else {
    /* also 0xFFFF (=interrupt enable) *** unchanged *** */
    if (addr>0x7F) io[addr]=value;

    /* CGB io handling */
    switch (addr) {
    case 0x4D: /* cpu speed switch */
      if (!colormode) switch2color();
      KEY1=(KEY1&0x80)|(value&1);
      break;
    case 0x4F: /* video bank selection */
      if (!colormode) switch2color();
      VBK=value&1;
      break;
    case 0x51: /* cgb DMA source hi */
      if (!colormode) switch2color();
      HDMA1=value;
      hdmasrc=((unsigned int)HDMA1<<8)|(HDMA2&0xF0);
      break;
    case 0x52: /* cgb DMA source low */
      if (!colormode) switch2color();
      HDMA2=value&0xF0;
      hdmasrc=((unsigned int)HDMA1<<8)|(HDMA2&0xF0);
      break;
    case 0x53: /* cgb DMA destination hi */
      if (!colormode) switch2color();
      HDMA3=value&0x1F;
      hdmadst=((unsigned int)(HDMA3&0x1F)<<8)|(HDMA4&0xF0);
      break;
    case 0x54: /* cgb DMA destination low */
      if (!colormode) switch2color();
      HDMA4=value&0xF0;
      hdmadst=((unsigned int)(HDMA3&0x1F)<<8)|(HDMA4&0xF0);
      break;
    case 0x55: /* cgb DMA start and counter */
      if (!colormode) switch2color();
      HDMA5=value;
      hdmasrc=((unsigned int)HDMA1<<8)|(HDMA2&0xF0);
      hdmadst=((unsigned int)(HDMA3&0x1F)<<8)|(HDMA4&0xF0);
      switch (hdmastarted) {  /* general=>1; horizontal=>2 stoping=>-1*/
      case 2:
	hdmastarted=value&0x80 ? 2 : -1;
	HDMA5&=0x7F;
	break;
      default:
	hdmastarted=((HDMA5&0x80)>>7)+1;
	if (hdmastarted==2) {
	  HDMA5&=0x7F;
	  break;
	}
	for (i=((HDMA5&0x7F)+1)<<4;i>0;i--,hdmadst++,hdmasrc++) 
	  vram[VBK&1][hdmadst&0x1FFF]=GetByteAt(hdmasrc);
	HDMA5=0x80;
	HDMA1=hdmasrc>>8;
	HDMA2=hdmasrc&0xF0;
	HDMA3=(hdmadst>>8)&0x1F;
	HDMA4=hdmadst&0xF0;
	hdmastarted=0;
	break;
      }

#ifdef DEBUG
      printf("HDMA5<= %02X (%i) (%04X=>%04X) at 0x%04X\n",value,
	     hdmastarted,hdmasrc,hdmadst+0x8000,Z80_REG.W.PC);
#endif
      /*      for (i=((HDMA5&0x7F)+1)<<4;i>0;i--,dma_d++,dma_s++) 
	      vram[VBK&1][dma_d&0x1FFF]=GetByteAt(dma_s);*/
      break;
    case 0x56: /* infrared IO */
      if (!colormode) switch2color();
#ifdef DEBUG
      printf("Fixme: infrared access (%02X at %04X).\n",value,Z80_REG.W.PC);
#endif
      break;
    case 0x68: /* BG pal write specs */
      if (!colormode) switch2color();
      BCPS=value&0xBF;
      u=GB_BGPAL[(value>>3)&7][(value>>1)&3];
      BCPD=value&1 ? (u>>8)&0x7F : u&0xFF;
      break;
    case 0x69: /* BG pal write data */
      if (!colormode) switch2color();
      i=BCPS;
      if (i&1) { /* high word */
	u=GB_BGPAL[(i>>3)&7][(i>>1)&3]=(GB_BGPAL[(i>>3)&7][(i>>1)&3]&0xFF)|
	  (value<<8);
      } else { /* low word */
	u=GB_BGPAL[(i>>3)&7][(i>>1)&3]=(GB_BGPAL[(i>>3)&7][(i>>1)&3]&0xFF00)|
	  value;
      }
      BGPAL[(i>>3)&7][(i>>1)&3]=color_translate(u);
      i=BCPS=i&0x80 ? (i&0x80)|((i+1)&0x3F) : i;
      u=GB_BGPAL[(i>>3)&7][(i>>1)&3];
      BCPD=i&1 ? u>>8 : u&0xFF;
      break;
    case 0x6A: /* OBJ pal write specs */
      if (!colormode) switch2color();
      OCPS=value&0xBF;
      u=GB_OBJPAL[(value>>3)&7][(value>>1)&3];
      OCPD=value&1 ? (u>>8)&0x7F : u&0xFF;
      break;
    case 0x6B: /* OBJ pal write data */
      if (!colormode) switch2color();
      i=OCPS;
      if (i&1) { /* high word */
	u=GB_OBJPAL[(i>>3)&7][(i>>1)&3]=(GB_OBJPAL[(i>>3)&7][(i>>1)&3]&0xFF)|
	  (value<<8);
      } else { /* low word */
	u=GB_OBJPAL[(i>>3)&7][(i>>1)&3]=(GB_OBJPAL[(i>>3)&7][(i>>1)&3]&0xFF00)|
	  value;
      }
      OBJPAL[(i>>3)&7][(i>>1)&3]=color_translate(u);
      i=OCPS=i&0x80 ? (i&0x80)|((i+1)&0x3F) : i;
      u=GB_OBJPAL[(i>>3)&7][(i>>1)&3];
      OCPD=i&1 ? u>>8 : u&0xFF;
      break;
    case 0x70: /* work bank selection */
      if (!colormode) switch2color();
      SVBK=value&7;
      workbanknr=value&7;
      if (workbanknr==0) workbanknr++;
#ifdef HARDDEBUG
      printf("WRAM bank %i selected.\n",workbanknr);
#endif
      break;
    }
  }
}

void SetByteAt(uint addr, uchar value)
{
#ifdef DEBUG
  if (addr==0xff59)
    fprintf(OUTSTREAM,"%04X=>%02X accessed at PC: %04X\n",
	    addr,value,Z80_REG.W.PC);
  /*      fprintf(OUTSTREAM,"%d%d ",(value>>5)&1,(value>>4)&1);*/
#endif

  switch (addr >> 8) {
  case 0xFF:IOOut(addr & 0xFF,value);return;
  default:
    switch (addr >> 13) {
    case 0:                                          /* RAM bank enable */
      if (mbctype==0) {
	fprintf(OUTSTREAM,"Fixme: No MBC and using SRAM ?\n");
	fprintf(OUTSTREAM,"       (written %02X to %04X)\n",value,addr);
	return;
      }
      if ((mbctype==2)&&((addr & 0x0100)>0)) {
	fprintf(OUTSTREAM,"Fixme: Using MBC2 and writing 0x%02X to 0x%04X.\n",
	       value,addr);
	return;
      }

      value&=0x0F;
#ifdef HARDDEBUG      
      if ((value!=10)&&(value!=0)) {
	fprintf(OUTSTREAM,"SRAM disable ? (0x%02X=>0x%04X)\n",value,addr);
      }
#endif

      rambankenabled= value == 10 ? 1 : 0;
      return;
    case 1:                                          /* ROM bank select */
      switch (mbctype) {
      case 0:
	if (value!=1) {
	  fprintf(OUTSTREAM,"Fixme: No MBC and using ROM switching ?\n");
	  fprintf(OUTSTREAM,"       (written %02X to %04X)\n",value,addr);
	  return;
	}
	break;
      case 1:
	if (mbcmode) 
	  rombanknr=value; else
	    rombanknr=(rombanknr&0x60)|(value & 0x1F);
	if (rombanknr>=romselectmask) {
	  fprintf(OUTSTREAM,"Fixme: ROM bank %d selected.\n",rombanknr);
	  return;
	}
	break;
      case 2:
	if ((addr & 0x0100)==0) {
	  fprintf(OUTSTREAM,
		  "Fixme: Using MBC2 and writing 0x%02X to 0x%04X.\n",
		  value,addr);
	  return;
	}
	if (mbcmode) 
	  rombanknr=value; else
	    rombanknr=(rombanknr&0x60)|(value & 0x1F);
	if (rombanknr>=romselectmask) {
	  fprintf(OUTSTREAM,"Fixme: ROM bank %d selected.\n",value);
	  return;
	}
	break;
      case 5:
	if (addr>=0x3000) break;
	if (value>=romselectmask) {
	  fprintf(OUTSTREAM,"Fixme: ROM bank %d selected (PC:%04X).\n",
		  value,Z80_REG.W.PC);
	  value&=romselectmask-1;
	}
	rombanknr=value&0x7F;
	break;
      }

      if (rombanknr==0) rombanknr++;
#ifdef HARDDEBUG
      fprintf(OUTSTREAM,"ROM select %d (PC:%04X).\n",rombanknr,Z80_REG.W.PC);
#endif
      return;
    case 2:                                        /* RAM/ROMHI bank select */
      if ((value&0xF0)>0) return;
      if (mbctype==2) {
	fprintf(OUTSTREAM,"Fixme: MBC2 and selecting RAM banks ?\n");
	fprintf(OUTSTREAM,"       (written %02X to %04X)\n",value,addr);
	return;
      }
      if ((mbcmode==0)&&(mbctype==1)) {                      /* ROM hi-addr */
	if (value>3) {
	  fprintf(OUTSTREAM,"Fixme: 16/8 mode, illegal hi address.\n");
	  fprintf(OUTSTREAM,"       (written %02X to %04X)\n",value,addr);
	  return;
	}
	rombanknr=(rombanknr & 0x1F)|((value&3)<<5);
	if (rombanknr>=romselectmask) {
	  fprintf(OUTSTREAM,"Fixme: ROM bank %d selected.\n",value);
	  return;
	}
#ifdef DEBUG
	fprintf(OUTSTREAM,"ROM bank %d selected (hi addr) PC:%04X.\n",
		rombanknr,Z80_REG.W.PC);
#endif	
	return;
      }
      rambanknr=value&3;

      if (rambanknr>=sramselectmask) {
	fprintf(OUTSTREAM,"Fixme: RAM bank %d selected.\n",rambanknr);
	fprintf(OUTSTREAM,"       (written %02X to %04X)\n",value,addr);
	return;
      }
#ifdef DEBUG
      fprintf(OUTSTREAM,"RAM bank %d selected.\n",rambanknr);
#endif      
      return;
    case 3:                                         /* MBC mode select */
      if ((value&0xF0)>0) return;
      if (mbctype==2) {
	fprintf(OUTSTREAM,"Fixme: MBC2 and selecting modes ?\n");
	fprintf(OUTSTREAM,"       (at PC: %04X written %02X to %04X)\n",
	       Z80_REG.W.PC,value,addr);
	return;
      }
      if (mbctype==5) {                          /* clock counter latch */
#ifdef DEBUG
	fprintf(OUTSTREAM,"Fixme: MBC5 clock funcs not implemented (%04X).\n",
		Z80_REG.W.PC);
#endif
	return;
      }
      mbcmode=value & 1;
#ifdef DEBUG
      fprintf(OUTSTREAM,"MBC mode %s selected.\n",mbcmode == 0 ? "16/8" : "4/32");
#endif
      return;
    case 4:                                        /* VRAM access */
      /* ************************************** */
      /* *** NOT CHECKING IF ACCESSIBLE !!! *** */
      /* ************************************** */
      /*      fprintf(OUTSTREAM,"VRAM access: %04X at PC %04X\n",addr,Z80_REG.W.PC); */
      vram[VBK&1][addr & 0x1FFF]=value;
      return;
    case 5:                                        /* SRAM access */
      value=ar_checkwrite(addr,value);

      if (rambankenabled!=0)
	sram[rambanknr][addr & 0x1FFF]=value;
      return;
    case 6:                                        /* internal RAM and OAM */
    case 7:                                        /* w/o IO (0xFF00-)     */
      if (addr<0xFE00) {
	/* internal RAM with echoing ! */

	value=ar_checkwrite((addr&0x1FFF)+0xC000,value);
	value=ar_checkwrite((addr&0x1FFF)+0xE000,value);
	switch (addr>>12) {
	case 0xC:
	case 0xE:
	  iram[0][addr & 0x0FFF]=value;
	  break;
	case 0xD:
	case 0xF:
	  iram[workbanknr][addr & 0x0FFF]=value;
	  break;
	}
      } else {
	/* OAM access */
	/* ************************************** */
	/* *** NOT CHECKING IF ACCESSIBLE !!! *** */
	/* ***    THIS COULD BE FATAL !!!     *** */
	/* ************************************** */
	oam[addr & 0xFF]=value;
      }
      return;
    }
  }
}

void switch2color(void)
{
  int k,l;
  if (force_stdgameboy) return;

  switch (bitmapbits) {
  case 8:
    scanline=cgb_scanline8;
    break;
  case 16:
    scanline=cgb_scanline16;
    break;
  case 32:
    scanline=cgb_scanline32;
    break;
  default:
    printf("Error: Cannot handle resolution %i, yet.\n",bitmapbits);
    exit(1);
  }
  for (k=0;k<8;k++)
    for (l=0;l<4;l++) {
      BGPAL[k][l]= color_translate(GB_BGPAL[k][l] );
      OBJPAL[k][l]=color_translate(GB_OBJPAL[k][l]);
    }
  
#ifndef DOS
  fprintf(OUTSTREAM,"Gameboy COLOR rom detected.\n");
#endif
  colormode=1;
}

/* for games with battery: saving routine */
void savestate(void)
{
  FILE *fp;
  int j,zeros,b;

  fprintf(OUTSTREAM,"Saving state to %s ... ",sramfile);
  fp=fopen(sramfile,"wb");
  if (fp) {
    zeros=0;j=0;
    while (j<sramsize) {
      b=sram[j / srambanksize][j % srambanksize];
      if (b==0) zeros++; 
      else {
	while (zeros) { fputc(0,fp);zeros--;}
	fputc(b,fp);
      }
      j++;
    }
    fclose(fp);
    if (zeros==sramsize) {
      fprintf(OUTSTREAM,"(empty) ");
      if (unlink(sramfile)) fprintf(OUTSTREAM,"erase FAILED, ");
    }
    fprintf(OUTSTREAM,"done.\n");
  } else {
    fprintf(OUTSTREAM,"ERROR\n");
  }
}

void tidyup(void)
{
  unsigned int i;

  donesys();

  fprintf(OUTSTREAM,"Freeing resources ... ");
  for (i=0;i<GB_WORKBANKCOUNT;i++) {
    if (iram[i]!=NULL) free(iram[i]);
  }
  if (oam!=NULL) free(oam);
  if (io!=NULL) free(io);
  if (vram[0]!=NULL) free(vram[0]);
  if (vram[1]!=NULL) free(vram[1]);
  out_ok;

  fprintf(OUTSTREAM,"Freeing ROM/RAM ...");
  for (i=0;i<sramselectmask;i++)
    if (sram[i]!=NULL) free(sram[i]);
  for (i=0;i<romselectmask;i++)
    if (bank[i]!=NULL) free(bank[i]);
  out_ok;
  return;
}

int initcart(char *cartfile)
{
  FILE *fp;
  unsigned int i,l;
  int j,k;
  char romname[17];
  char filename[512];
  uint cs=0,ccs=0;

#ifdef UNIX
#define CCLOSE if (!strcmp(&cartfile[strlen(cartfile)-3],".gz")) pclose(fp); else \
fclose(fp)
#else
#define CCLOSE fclose(fp)
#endif

#ifdef VERBOSE
  fprintf(OUTSTREAM,"Opening cart: %s\n",cartfile);
#endif

  for (i=0;i<GB_MAXROMBANKCOUNT;i++)
    bank[i]=NULL;

  for (i=0;i<GB_WORKBANKCOUNT;i++)
    iram[i]=NULL;

  oam=NULL;io=NULL;
  vram[0]=NULL;vram[1]=NULL;
  for (i=0;i<sramselectmask;i++)
    sram[i]=NULL;
  for (i=0;i<romselectmask;i++)
    bank[i]=NULL;

#ifdef UNIX
  if (!strcmp(&cartfile[strlen(cartfile)-3],".gz")) {
    fprintf(OUTSTREAM,"Opening gzipped.\n");
    sprintf(filename,"gunzip -c \"%s\"",cartfile);
    if ((fp=popen(filename,"r"))==NULL) {
      fprintf(stderr,"Failed to open gzipped cartridge.\n");
      exit(1);
    }

    pclose(fp);

    if ((fp=popen(filename,"r"))==NULL) {
      fprintf(stderr,"Failed to open gzipped cartridge.\n");
      exit(1);
    }
  } else {
#endif
    if ((fp=fopen(cartfile,"r"))==NULL) {
      sprintf(filename,"%s.gb",cartfile);
      if ((fp=fopen(filename,"r"))==NULL) {
	sprintf(filename,"%s.GB",cartfile);
	if ((fp=fopen(filename,"r"))==NULL) {
	  fprintf(stderr,"Failed to open cartridge.\n");
	  exit(1);
	}
      }
    }
#ifdef UNIX
  }
#endif

  /* alloc ROM banks */
  i=0;
  while (!feof(fp)) {
    if (i>=GB_MAXROMBANKCOUNT) {
      fprintf(stderr,"The cartridge is too big.\n");
      CCLOSE;
      return 1;
    }
    bank[i]=malloc(GB_ROMBANKSIZE);
    if (bank[i]==NULL) {
      fprintf(stderr,"Out of memory.\n");
      CCLOSE;
      exit(1);
    }

    if ((l=fread(bank[i],1,GB_ROMBANKSIZE,fp))==0) break;

    if (l!=GB_ROMBANKSIZE) {
      fprintf(stderr,"error reading data (reading %i,returned: %d).\n",i,l);
      exit(1);
    }
    
    if (i==0) {
      ccs=(((uint)bank[0][0x14E]<< 8)|
                  bank[0][0x14F]);
      cs=0;
      cs-=bank[0][0x14E];
      cs-=bank[0][0x14F];
    }

    for (k=0;k<GB_ROMBANKSIZE;k++) cs+=bank[i][k];
    i++;
  }
  romselectmask=j=i;
  CCLOSE;

  if (ccs!=cs) {
    out_failed;
    fprintf(stderr,"Checksum failure (is: %04X should be: %04X).\n",cs,ccs);
    fprintf(stderr,"Cartridge file broken ?\n");
    return 1;
  } else out_ok;

  /* cart check --- nintendo logo */
  
  for (i=0;i<0x30;i++)
    if (bank[0][0x104+i]!=Nintendo_Logo[i])
      {
	fprintf(stderr,"I don't recognize this rom.\n");
	fprintf(stderr,"Nintendo logo not found.\n");
	return 1;
      }
  
  for (i=0;i<16;i++)
    romname[i]=bank[0][0x134+i];
  romname[16]=0;
  fprintf(OUTSTREAM,"Rom: %s\nCheck complete.\n",romname);

  /* figure out the MBC type */
  fprintf(OUTSTREAM,"MBC type (%02X): ",bank[0][0x147]);
  switch (bank[0][0x147]) {
  case 0:
    fprintf(OUTSTREAM,"ROM ONLY");
    mbctype=0;
    break;
  case 1:
    mbctype=1;
    fprintf(OUTSTREAM,"ROM+MBC1");
    break;
  case 2:
    mbctype=1;
    fprintf(OUTSTREAM,"ROM+MBC1+SRAM");
    break;
  case 3:
    mbctype=1;
    fprintf(OUTSTREAM,"ROM+MBC1+RAM+BATTERY");
    break;
  case 5:
    mbctype=2;
    fprintf(OUTSTREAM,"ROM+MBC2");
    break;
  case 6:
    mbctype=2;
    fprintf(OUTSTREAM,"ROM+MBC2+BATTERY");
    break;
  case 8:
    mbctype=0;
    fprintf(OUTSTREAM,"ROM+SRAM");
    break;
  case 9:
    mbctype=0;
    fprintf(OUTSTREAM,"ROM+SRAM+BATTERY");
    break;
  case 0x10:
  case 0x13:
  case 0x1C:
    mbctype=5;
    fprintf(OUTSTREAM,"ROM+MBC5 (?)");
    break;
  case 0x19:
    mbctype=5;
    fprintf(OUTSTREAM,"ROM+MBC5");
    break;
  case 0x1A:
    mbctype=5;
    fprintf(OUTSTREAM,"ROM+SRAM+MBC5");
    break;
  case 0x1B:
  case 0x1E:
    mbctype=5;
    fprintf(OUTSTREAM,"MBC5+ROM+SRAM+BATTERY");
    break;
  case 255:                  /* HuC1 - could be buggy */
    mbctype=1;
    fprintf(OUTSTREAM,"Warning: HuC1 detected ... this is not supported.");
    break;
  default:
    fprintf(stderr,
	     "\nI don't know this cartridge type (%02X). Sorry ...\n",
	   bank[0][0x147]);
    fprintf(stderr,"You can report this to me. (Thanks)\n");
    return 1;
  }
  fprintf(OUTSTREAM,"\n");

  /* allocate RAMs 'n co */
  vram[0]=calloc(GB_VRAMSIZE,1);
  vram[1]=calloc(GB_VRAMSIZE,1);
  io=calloc(GB_IOAREASIZE,1);
  oam=calloc(GB_OAMSIZE,1);
  for (i=0;i<GB_WORKBANKCOUNT;i++)
    if ((iram[i]=calloc(GB_WORKRAMBANKSIZE,1))==NULL) {
      fprintf(OUTSTREAM,"Failed to allocate 4kB WORKRAM.\n");
      return 1;
    }

  if ((vram[0]==NULL)||(vram[1]==NULL)||(io==NULL)||(oam==NULL))
    return 1;

  /* alloc SRAM banks */

  srambanksize=0x2000;
  for (i=0;i<4;i++) {
    if ((sram[i]=calloc(srambanksize,1))==NULL) {
      fprintf(OUTSTREAM,"Failed to allocate 8kB SRAM.\n");
      return 1;
    }
  }
  sramsize=0x8000;sramselectmask=4;

#ifdef VERBOSE
  fprintf(OUTSTREAM,"Allocated SRAM (%d kB).\n",sramsize/1024);
#endif

  strcpy(sramfile,cartfile);
  *strrchr(sramfile,'.')=0;
  strcat(sramfile,".GBS");
  fprintf(OUTSTREAM,"Trying to open SRAM-file '%s' ... ",sramfile);
  fp=fopen(sramfile,"rb");
  if (fp!=NULL) {
    fseek(fp,0,SEEK_END);
    j=ftell(fp);rewind(fp);
    if (j<=sramsize) {
      k=GB_SRAMBANKCOUNT[bank[0][0x149]];
      if (k==0) k++;
      i=0;
      while (j>0) {
	l=j>srambanksize ? srambanksize : j;
	cs=fread(sram[i++],1,l,fp);
	if (cs<l) {
	  fprintf(OUTSTREAM,"read error (wanted %d, got %d)\n",l,cs);
	  CCLOSE;
	  return 1;
	}
	j-=l;
      }
      out_ok;
    } else {
      fprintf(OUTSTREAM,"corrupt state\n");
    }
    CCLOSE;
  } else {
    fprintf(OUTSTREAM,"not found\n");
  }
  

  /* mem init */
  for (i=0;i<0x100;i++)
    io[i]=Area_0xFF00_init[i];

  /* init registers */
  Z80_REG.W.AF= force_stdgameboy ? 0x01B0 : 0x11B0;

  Z80_REG.W.BC=0x0013;
  Z80_REG.W.DE=0x00D8;
  Z80_REG.W.HL=0x014D;
  Z80_REG.W.SP=0xFFFE;
  Z80_REG.W.PC=0x0100;
  Z80_HALTED=Z80_IE=0;
  Z80_CPUCLKS=0;

  /* and stuff */
  rombanknr=1;workbanknr=1;
  rambanknr=0;
  rambankenabled=0;
  colormode=mbcmode=0;
  newjoypadstate=joypadstate=0xFF;
  timerclks=timerfreq=GB_TIMERFRQ[TAC&0x03];
  dividerclks=GB_DIVLOAD;
  lcdphase=0;STAT|=2;     /* lcd phase 2 */
  lcdclks=GB_VCLKS[0];
  IE=0x00;/* IM=0xFF; */
  ABORT_EMULATION=0;
  joypadclks=GB_JOYCLKS;
  soundcounter=-1;
  vblankdelay=GB_VBLANKDELAYCLKS;
  vblankoccured=0;
  SVBK=VBK=KEY1=0x00;
  hdmastarted=hdmasrc=hdmadst=0;
  snd_updateclks=0xFFFF;
  serialioclks=0;

  if (!(i=initsys())) {
    switch (bitmapbits) {
    case 8:scanline=gb_scanline8;break;
    case 16:scanline=gb_scanline16;break;
    case 32:scanline=gb_scanline32;break;
    default:
      printf("Error: Cannot handle resolution %i\n",bitmapbits);
      return 1;
    }
  }

  return i;
}

void SetWordAt(uint addr,uint value)
{
  SetByteAt(addr,(uchar)(value & 0xFF));
  SetByteAt(addr+1,(uchar)(value >> 8));
}

#define do_tilepix(pixeltype, pack) \
{\
	unsigned int zpos;\
	zpos=(unsigned int)x;\
\
	if (!doublesize) {\
		pixeltype *p; \
\
		p=(pixeltype *)lcdbuffer+y+x;\
\
		*p++=FPAL[zbuffer[zpos++]=tilepix  >> 14];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >>  6)&0x03];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >> 12)&0x03];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >>  4)&0x03];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >> 10)&0x03];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >>  2)&0x03];\
		*p++=FPAL[zbuffer[zpos++]=(tilepix >>  8)&0x03];\
		*p=FPAL[zbuffer[zpos]=tilepix        &0x03];\
	} else {\
		pixeltype *p; \
\
		p=(pixeltype *)lcdbuffer+y+(x<<1);\
\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=tilepix  >> 14]; \
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >>  6)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >> 12)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >>  4)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >> 10)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >>  2)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos++]=(tilepix >> 8)&0x03];\
		p+=pack; \
		\
		*(p+1)=*p=\
			FPAL[zbuffer[zpos]=tilepix&0x03];\
		\
	}\
}

void gb_scanline8(void)
{
  int x,y,tx,ty,xc,tmp;
  uchar *tileofs,*tiledata,*sprofs;
  uchar tile,tile1,tile2;
  uint tilepix;
  uchar zbuffer[GB_XBUFFERSIZE];
  uchar FPAL[4];

  FPAL[0]=(uchar)GB_STDPAL[BGP & 0x03];
  FPAL[1]=(uchar)GB_STDPAL[(BGP >> 2)& 0x03];
  FPAL[2]=(uchar)GB_STDPAL[(BGP >> 4)& 0x03];
  FPAL[3]=(uchar)GB_STDPAL[BGP >> 6];


  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */


    tileofs=vram[0]
      +((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;

    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[0]+(int)tile*16 
	: vram[0]+0x1000+(int)((signed char)tile)*16;
      tiledata+=((LY+SCY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(uchar, 2);

    }
  } else { /* if BG not enabled */
	  memset(zbuffer,0,GB_XBUFFERSIZE);
	  memset((uchar*)lcdbuffer+y, FPAL[0],
		doublesize ? GB_XBUFFERSIZE<<1 : GB_XBUFFERSIZE);
  }


  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tileofs=vram[0]+(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);

    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tiledata=LCDC & 0x10 ? vram[0]+(int)tileofs[tx]*16 
	: vram[0]+0x1000+(int)((signed char)tileofs[tx])*16;
      tiledata+=((LY-WY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(uchar, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* background sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((sprofs[3]&0x80)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[0]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	if (sprofs[3]&0x10) {
	  FPAL[0]=(uchar)GB_STDPAL[OBP1 & 0x03];
	  FPAL[1]=(uchar)GB_STDPAL[(OBP1 >> 2)& 0x03];
	  FPAL[2]=(uchar)GB_STDPAL[(OBP1 >> 4)& 0x03];
	  FPAL[3]=(uchar)GB_STDPAL[OBP1 >> 6];
	} else {
	  FPAL[0]=(uchar)GB_STDPAL[OBP0 & 0x03];
	  FPAL[1]=(uchar)GB_STDPAL[(OBP0 >> 2)& 0x03];
	  FPAL[2]=(uchar)GB_STDPAL[(OBP0 >> 4)& 0x03];
	  FPAL[3]=(uchar)GB_STDPAL[OBP0 >> 6];
	}
	
	if (!doublesize ) {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uchar *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uchar *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uchar *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uchar *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uchar *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uchar *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uchar *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uchar *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uchar *)lcdbuffer)[y+(x<<1)  ]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+2]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+4]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+6]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+8]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+10]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+12]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+14]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}
      }
    }

    /* foreground sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if (((sprofs[3]&0x80)==0)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[0]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	if (sprofs[3]&0x10) {
	  FPAL[0]=(uchar)GB_STDPAL[OBP1 & 0x03];
	  FPAL[1]=(uchar)GB_STDPAL[(OBP1 >> 2)& 0x03];
	  FPAL[2]=(uchar)GB_STDPAL[(OBP1 >> 4)& 0x03];
	  FPAL[3]=(uchar)GB_STDPAL[OBP1 >> 6];
	} else {
	  FPAL[0]=(uchar)GB_STDPAL[OBP0 & 0x03];
	  FPAL[1]=(uchar)GB_STDPAL[(OBP0 >> 2)& 0x03];
	  FPAL[2]=(uchar)GB_STDPAL[(OBP0 >> 4)& 0x03];
	  FPAL[3]=(uchar)GB_STDPAL[OBP0 >> 6];
	}

	if (!doublesize) {
	  if (tilepix>>14)
	    ((uchar *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uchar *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uchar *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uchar *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uchar *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uchar *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uchar *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uchar *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if (tilepix>>14)
	    ((uchar *)lcdbuffer)[y+(x<<1)  ]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+2]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+4]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+6]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+8]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+10]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+12]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+14]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}
      }
    }


  }
}

void cgb_scanline8(void)
{
  int x,y,tx,ty,xc,tmp;
  uchar *tileofs1,*tileofs2,*tiledata,*sprofs;
  uchar tile,tile1,tile2,tile_a;
  uint tilepix,tilenum;
  uchar zbuffer[GB_XBUFFERSIZE];
  uchar FPAL[4];

  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */
    tilenum=((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;
    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs1[(tx+(SCX>>3))&0x1F];
      tile_a=tileofs2[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tile<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tile)<<4);
      tiledata+=(tile_a&0x40) ? (7-((LY+SCY)&7))<<1:((LY+SCY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=(uchar)BGPAL[tile_a&7][0];
      FPAL[1]=(uchar)BGPAL[tile_a&7][1];
      FPAL[2]=(uchar)BGPAL[tile_a&7][2];
      FPAL[3]=(uchar)BGPAL[tile_a&7][3];

	do_tilepix(uchar, 2);
    }
  } else { /* if BG not enabled */
    memset(zbuffer,0,GB_XBUFFERSIZE);
	  memset((uchar*)lcdbuffer+y, 0, /* 0 or background ? */
		doublesize ? GB_XBUFFERSIZE<<1 : GB_XBUFFERSIZE);
  }
  
  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tilenum=(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tile_a=tileofs2[tx];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tileofs1[tx]<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tileofs1[tx])<<4);

      tiledata+=(tile_a&0x40) ? (7-((LY-WY)&7))<<1:((LY-WY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=(uchar)BGPAL[tile_a&7][0];
      FPAL[1]=(uchar)BGPAL[tile_a&7][1];
      FPAL[2]=(uchar)BGPAL[tile_a&7][2];
      FPAL[3]=(uchar)BGPAL[tile_a&7][3];

	do_tilepix(uchar, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* background sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((sprofs[3]&0x80)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=(uchar)OBJPAL[sprofs[3]&7][0];
	FPAL[1]=(uchar)OBJPAL[sprofs[3]&7][1];
	FPAL[2]=(uchar)OBJPAL[sprofs[3]&7][2];
	FPAL[3]=(uchar)OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uchar *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uchar *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uchar *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uchar *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uchar *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uchar *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uchar *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uchar *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uchar *)lcdbuffer)[y+(x<<1)  ]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+2]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+4]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+6]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+8]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+10]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+12]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uchar *)lcdbuffer)[y+(x<<1)+14]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}
      }
    }

    /* foreground sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if (((sprofs[3]&0x80)==0)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=(uchar)OBJPAL[sprofs[3]&7][0];
	FPAL[1]=(uchar)OBJPAL[sprofs[3]&7][1];
	FPAL[2]=(uchar)OBJPAL[sprofs[3]&7][2];
	FPAL[3]=(uchar)OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if (tilepix>>14)
	    ((uchar *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uchar *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uchar *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uchar *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uchar *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uchar *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uchar *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uchar *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if (tilepix>>14)
	    ((uchar *)lcdbuffer)[y+(x<<1)  ]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+2]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+4]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+6]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+8]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+10]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+12]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uchar *)lcdbuffer)[y+(x<<1)+14]=
	      ((uchar *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}
      }
    }


  }
}

void gb_scanline16(void)
{
  int x,y,tx,ty,xc,tmp;
  uchar *tileofs,*tiledata,*sprofs;
  uchar tile,tile1,tile2;
  uint tilepix;
  uchar zbuffer[GB_XBUFFERSIZE];
  uint FPAL[4];

  FPAL[0]=GB_STDPAL[BGP & 0x03];
  FPAL[1]=GB_STDPAL[(BGP >> 2)& 0x03];
  FPAL[2]=GB_STDPAL[(BGP >> 4)& 0x03];
  FPAL[3]=GB_STDPAL[BGP >> 6];


  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */
    tileofs=vram[0]
      +((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;
    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[0]+((int)tile<<4) 
	: vram[0]+0x1000+((int)((signed char)tile)<<4);
      tiledata+=((LY+SCY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(uint, 2);
    }
  } else { /* if BG not enabled */

	uint *p;

    memset(zbuffer,0,GB_XBUFFERSIZE);

	p=(uint *)lcdbuffer+y;
    if (doublesize)
      for (x=0;x<GB_XBUFFERSIZE*2;x++) *p++=FPAL[0];
    else
      for (x=0;x<GB_XBUFFERSIZE;x++) *p++=FPAL[0];
  }
  
  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tileofs=vram[0]+(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);
    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tiledata=LCDC & 0x10 ? vram[0]+((int)tileofs[tx]<<4) 
	: vram[0]+0x1000+((int)((signed char)tileofs[tx])<<4);
      tiledata+=((LY-WY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(uint, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* background sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((sprofs[3]&0x80)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[0]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	if (sprofs[3]&0x10) {
	  FPAL[0]=GB_STDPAL[OBP1 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP1 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP1 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP1 >> 6];
	} else {
	  FPAL[0]=GB_STDPAL[OBP0 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP0 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP0 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP0 >> 6];
	}

	if (!doublesize) {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uint *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uint *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uint *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uint *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uint *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uint *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uint *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uint *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uint *)lcdbuffer)[y+(x<<1)  ]=
	      ((uint *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+2]=
	      ((uint *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+4]=
	      ((uint *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+6]=
	      ((uint *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+8]=
	      ((uint *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+10]=
	      ((uint *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+12]=
	      ((uint *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+14]=
	      ((uint *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	
      }
    }

    /* foreground sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if (((sprofs[3]&0x80)==0)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[0]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	if (sprofs[3]&0x10) {
	  FPAL[0]=GB_STDPAL[OBP1 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP1 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP1 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP1 >> 6];
	} else {
	  FPAL[0]=GB_STDPAL[OBP0 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP0 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP0 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP0 >> 6];
	}

	if (!doublesize) {
	  if (tilepix>>14)
	    ((uint *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uint *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uint *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uint *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uint *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uint *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uint *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uint *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if (tilepix>>14)
	    ((uint *)lcdbuffer)[y+(x<<1)  ]=
	      ((uint *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+2]=
	      ((uint *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+4]=
	      ((uint *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+6]=
	      ((uint *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+8]=
	      ((uint *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+10]=
	      ((uint *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+12]=
	      ((uint *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+14]=
	      ((uint *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	  
      }
    }


  }
}

void cgb_scanline16(void)
{
  int x,y,tx,ty,xc,tmp;
  uchar *tileofs1,*tileofs2,*tiledata,*sprofs;
  uchar tile,tile1,tile2,tile_a;
  uint tilepix,tilenum;
  uchar zbuffer[GB_XBUFFERSIZE];
  uint FPAL[4];

  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */
    tilenum=((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;
    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs1[(tx+(SCX>>3))&0x1F];
      tile_a=tileofs2[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tile<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tile)<<4);
      tiledata+=(tile_a&0x40) ? (7-((LY+SCY)&7))<<1:((LY+SCY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=BGPAL[tile_a&7][0];
      FPAL[1]=BGPAL[tile_a&7][1];
      FPAL[2]=BGPAL[tile_a&7][2];
      FPAL[3]=BGPAL[tile_a&7][3];

	do_tilepix(uint, 2);
    }
  } else { /* if BG not enabled */
	uint *p;

    memset(zbuffer,0,GB_XBUFFERSIZE);

	p=(uint *)lcdbuffer+y;
    if (doublesize)
      for (x=0;x<GB_XBUFFERSIZE*2;x++) *p++=0; /* 0 or background ? */
    else
      for (x=0;x<GB_XBUFFERSIZE;x++) *p++=0; /* 0 or background ? */

  }
  
  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tilenum=(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tile_a=tileofs2[tx];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tileofs1[tx]<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tileofs1[tx])<<4);

      tiledata+=(tile_a&0x40) ? (7-((LY-WY)&7))<<1:((LY-WY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=BGPAL[tile_a&7][0];
      FPAL[1]=BGPAL[tile_a&7][1];
      FPAL[2]=BGPAL[tile_a&7][2];
      FPAL[3]=BGPAL[tile_a&7][3];

	do_tilepix(uint, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* background sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((sprofs[3]&0x80)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=OBJPAL[sprofs[3]&7][0];
	FPAL[1]=OBJPAL[sprofs[3]&7][1];
	FPAL[2]=OBJPAL[sprofs[3]&7][2];
	FPAL[3]=OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uint *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uint *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uint *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uint *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uint *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uint *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uint *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uint *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((uint *)lcdbuffer)[y+(x<<1)  ]=
	      ((uint *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+2]=
	      ((uint *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+4]=
	      ((uint *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+6]=
	      ((uint *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+8]=
	      ((uint *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+10]=
	      ((uint *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+12]=
	      ((uint *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((uint *)lcdbuffer)[y+(x<<1)+14]=
	      ((uint *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	  
      }
    }

    /* foreground sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if (((sprofs[3]&0x80)==0)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=OBJPAL[sprofs[3]&7][0];
	FPAL[1]=OBJPAL[sprofs[3]&7][1];
	FPAL[2]=OBJPAL[sprofs[3]&7][2];
	FPAL[3]=OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if (tilepix>>14)
	    ((uint *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uint *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uint *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uint *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uint *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uint *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uint *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uint *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if (tilepix>>14)
	    ((uint *)lcdbuffer)[y+(x<<1)  ]=
	      ((uint *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+2]=
	      ((uint *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+4]=
	      ((uint *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+6]=
	      ((uint *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+8]=
	      ((uint *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+10]=
	      ((uint *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+12]=
	      ((uint *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((uint *)lcdbuffer)[y+(x<<1)+14]=
	      ((uint *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	  
      }
    }

  }
}

void gb_scanline32(void)
{
  register int x,y,tx,ty,xc,tmp;
  register uchar *tileofs,*tiledata,*sprofs;
  register uchar tile,tile1,tile2;
  register uint tilepix;
  register ulong *lcdbuf;
  register uchar *zbuf;
  uchar zbuffer[GB_XBUFFERSIZE];
  ulong FPAL[4];

  FPAL[0]=GB_STDPAL[BGP & 0x03];
  FPAL[1]=GB_STDPAL[(BGP >> 2)& 0x03];
  FPAL[2]=GB_STDPAL[(BGP >> 4)& 0x03];
  FPAL[3]=GB_STDPAL[BGP >> 6];

  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */
    tileofs=vram[0]
      +((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;
    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[0]+((int)tile<<4) 
	: vram[0]+0x1000+((int)((signed char)tile)<<4);
      tiledata+=((LY+SCY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(ulong, 2);
    }
  } else { /* if BG not enabled */
	ulong *p;

    memset(zbuffer,0,GB_XBUFFERSIZE);

	p=(ulong *)lcdbuffer+y;
    if (doublesize)
      for (x=0;x<GB_XBUFFERSIZE*2;x++) *p++=FPAL[0];
    else
      for (x=0;x<GB_XBUFFERSIZE;x++) *p++=FPAL[0];

  }
  
  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tileofs=vram[0]+(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);
    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tiledata=LCDC & 0x10 ? vram[0]+((int)tileofs[tx]<<4) 
	: vram[0]+0x1000+((int)((signed char)tileofs[tx])<<4);
      tiledata+=((LY-WY)&7)<<1;
      tilepix=((uint)(tiledata[1]&0xAA)<<8)|((uint)(tiledata[0]&0xAA)<<7)|
	(tiledata[0]&0x55)|((tiledata[1]&0x55)<<1);

	do_tilepix(ulong, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((tmp>=0)&&(tmp<ty)&&(sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[0]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	if (sprofs[3]&0x10) {
	  FPAL[0]=GB_STDPAL[OBP1 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP1 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP1 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP1 >> 6];
	} else {
	  FPAL[0]=GB_STDPAL[OBP0 & 0x03];
	  FPAL[1]=GB_STDPAL[(OBP0 >> 2)& 0x03];
	  FPAL[2]=GB_STDPAL[(OBP0 >> 4)& 0x03];
	  FPAL[3]=GB_STDPAL[OBP0 >> 6];
	}
	
	if (sprofs[3]&0x80) {

	  if (!doublesize) {
	    lcdbuf=(ulong*)lcdbuffer+x+y;
	    zbuf=zbuffer+x;
	    if ((!*zbuf++)&&(tilepix>>14))
	      *lcdbuf=FPAL[tilepix >> 14];
	    if ((!*zbuf++)&&((tilepix>>6)&0x03))
	      *(lcdbuf+1)=FPAL[(tilepix >> 6)&0x03];
	    if ((!*zbuf++)&&((tilepix>>12)&0x03))
	      *(lcdbuf+2)=FPAL[(tilepix >> 12)&0x03];
	    if ((!*zbuf++)&&((tilepix>>4)&0x03))
	      *(lcdbuf+3)=FPAL[(tilepix >> 4)&0x03];
	    if ((!*zbuf++)&&((tilepix>>10)&0x03))
	      *(lcdbuf+4)=FPAL[(tilepix >> 10)&0x03];
	    if ((!*zbuf++)&&((tilepix>>2)&0x03))
	      *(lcdbuf+5)=FPAL[(tilepix >> 2)&0x03];
	    if ((!*zbuf++)&&((tilepix>>8)&0x03))
	      *(lcdbuf+6)=FPAL[(tilepix >> 8)&0x03];
	    if ((!*zbuf++)&&(tilepix&0x03))
	      *(lcdbuf+7)=FPAL[tilepix&0x03];
	  } else {
	    if ((zbuffer[x]==0)&&(tilepix>>14))
	      ((ulong *)lcdbuffer)[y+(x<<1)  ]=
		((ulong *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	    if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+2]=
		((ulong *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	    if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+4]=
		((ulong *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	    if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+6]=
		((ulong *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	    if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+8]=
		((ulong *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	    if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+10]=
		((ulong *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	    if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+12]=
		((ulong *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	    if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	      ((ulong *)lcdbuffer)[y+(x<<1)+14]=
		((ulong *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	  }
	} else {

	  if (!doublesize) {
	    lcdbuf=(ulong*)lcdbuffer+x+y;
	    if (tilepix>>14)
	      *lcdbuf=FPAL[tilepix >> 14];
	    if ((tilepix>>6)&0x03)
	      *(lcdbuf+1)=FPAL[(tilepix >> 6)&0x03];
	    if ((tilepix>>12)&0x03)
	      *(lcdbuf+2)=FPAL[(tilepix >> 12)&0x03];
	    if ((tilepix>>4)&0x03)
	      *(lcdbuf+3)=FPAL[(tilepix >> 4)&0x03];
	    if ((tilepix>>10)&0x03)
	      *(lcdbuf+4)=FPAL[(tilepix >> 10)&0x03];
	    if ((tilepix>>2)&0x03)
	      *(lcdbuf+5)=FPAL[(tilepix >> 2)&0x03];
	    if ((tilepix>>8)&0x03)
	      *(lcdbuf+6)=FPAL[(tilepix >> 8)&0x03];
	    if (tilepix&0x03)
	      *(lcdbuf+7)=FPAL[tilepix&0x03];
	  } else {
	    if (tilepix>>14)
	      ((ulong *)lcdbuffer)[y+(x<<1)  ]=
		((ulong *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	    if ((tilepix>>6)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+2]=
		((ulong *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	    if ((tilepix>>12)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+4]=
		((ulong *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	    if ((tilepix>>4)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+6]=
		((ulong *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	    if ((tilepix>>10)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+8]=
		((ulong *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	    if ((tilepix>>2)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+10]=
		((ulong *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	    if ((tilepix>>8)&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+12]=
		((ulong *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	    if (tilepix&0x03)
	      ((ulong *)lcdbuffer)[y+(x<<1)+14]=
		((ulong *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	  }
	}
      }
    }
  }
}

void cgb_scanline32(void)
{
  int x,y,tx,ty,xc,tmp;
  uchar *tileofs1,*tileofs2,*tiledata,*sprofs;
  uchar tile,tile1,tile2,tile_a;
  uint tilepix,tilenum;
  uchar zbuffer[GB_XBUFFERSIZE];
  ulong FPAL[4];

  y=LY*GB_XBUFFERSIZE;
  if (doublesize) y<<=2;

  if (LCDC & 0x01) { /* if BG enabled */
    tilenum=((((LY+SCY)&0xFF) >> 3)<<5)+(LCDC & 0x08 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    xc=((LCDC & 0x20)&&(WY<=LY) ? ((WX<7 ? 0 : WX-7) >> 3) : 21);
    xc=xc>21 ? 21 : xc;
    for (tx=0,x=8-(SCX & 7);tx<xc;tx++,x+=8) {
      tile=tileofs1[(tx+(SCX>>3))&0x1F];
      tile_a=tileofs2[(tx+(SCX>>3))&0x1F];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tile<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tile)<<4);
      tiledata+=(tile_a&0x40) ? (7-((LY+SCY)&7))<<1:((LY+SCY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=BGPAL[tile_a&7][0];
      FPAL[1]=BGPAL[tile_a&7][1];
      FPAL[2]=BGPAL[tile_a&7][2];
      FPAL[3]=BGPAL[tile_a&7][3];

	do_tilepix(ulong, 2);
    }
  } else { /* if BG not enabled */
	ulong *p;

    memset(zbuffer,0,GB_XBUFFERSIZE);

	p=(ulong *)lcdbuffer+y;
    if (doublesize)
      for (x=0;x<GB_XBUFFERSIZE*2;x++) *p++=0; /* 0 or background ? */
    else
      for (x=0;x<GB_XBUFFERSIZE;x++) *p++=0; /* 0 or background ? */

  }
  
  if ((LCDC & 0x20)&&(WY<=LY)&&(WX<167)) { /* if window is on and visible */

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    x=WX+1;xc=167-WX;
    tilenum=(((LY-WY) >> 3)<<5)+(LCDC & 0x40 ? 0x1C00 : 0x1800);
    tileofs1=vram[0]+tilenum;
    tileofs2=vram[1]+tilenum;
    for (tx=0;xc>0;xc-=8,x+=8,tx++) {
      tile_a=tileofs2[tx];
      tiledata=LCDC & 0x10 ? vram[(tile_a>>3)&1]+((int)tileofs1[tx]<<4) 
	: vram[(tile_a>>3)&1]+0x1000+((int)((signed char)tileofs1[tx])<<4);

      tiledata+=(tile_a&0x40) ? (7-((LY-WY)&7))<<1:((LY-WY)&7)<<1;
      tile1=tiledata[0];tile2=tiledata[1];
      if (tile_a&0x20) {tile1=H_Flip[tile1];tile2=H_Flip[tile2];}

      tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	(tile1&0x55)|((tile2&0x55)<<1);

      FPAL[0]=BGPAL[tile_a&7][0];
      FPAL[1]=BGPAL[tile_a&7][1];
      FPAL[2]=BGPAL[tile_a&7][2];
      FPAL[3]=BGPAL[tile_a&7][3];

	do_tilepix(ulong, 2);
    }
  }

  if (LCDC & 0x02) { /* sprites enabled */
    ty=LCDC & 0x04 ? 16 : 8;

    y=LY*GB_XBUFFERSIZE;
    if (doublesize) y<<=2;

    /* background sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if ((sprofs[3]&0x80)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=OBJPAL[sprofs[3]&7][0];
	FPAL[1]=OBJPAL[sprofs[3]&7][1];
	FPAL[2]=OBJPAL[sprofs[3]&7][2];
	FPAL[3]=OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((ulong *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((ulong *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((ulong *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((ulong *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((ulong *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((ulong *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((ulong *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((ulong *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if ((zbuffer[x]==0)&&(tilepix>>14))
	    ((ulong *)lcdbuffer)[y+(x<<1)  ]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((zbuffer[x+1]==0)&&((tilepix>>6)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+2]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((zbuffer[x+2]==0)&&((tilepix>>12)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+4]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((zbuffer[x+3]==0)&&((tilepix>>4)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+6]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((zbuffer[x+4]==0)&&((tilepix>>10)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+8]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((zbuffer[x+5]==0)&&((tilepix>>2)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+10]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((zbuffer[x+6]==0)&&((tilepix>>8)&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+12]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if ((zbuffer[x+7]==0)&&(tilepix&0x03))
	    ((ulong *)lcdbuffer)[y+(x<<1)+14]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	
      }
    }

    /* foreground sprites */
    for (xc=GB_SPRITECOUNT-1,sprofs=oam+0x9C;xc>=0;xc--,sprofs-=4) {
      tmp=LY-sprofs[0]+16;
      if (((sprofs[3]&0x80)==0)&&(tmp>=0)&&(tmp<ty)&&
	  (sprofs[1]<GB_LCDXSCREENSIZE+8)) {
	if (sprofs[3]&0x40) tmp=ty-1-tmp;
	tiledata=vram[(sprofs[3]>>3)&1]
	  +(((uint)(sprofs[2] & (LCDC & 0x04 ? 0xFE : 0xFF)))<<4)
	  +(tmp<<1);
	if (sprofs[3]&0x20) {
	  tile1=H_Flip[tiledata[0]];
	  tile2=H_Flip[tiledata[1]];
	} else {
	  tile1=tiledata[0];
	  tile2=tiledata[1];
	}
	tilepix=((uint)(tile2&0xAA)<<8)|((uint)(tile1&0xAA)<<7)|
	  (tile1&0x55)|((tile2&0x55)<<1);
	x=sprofs[1];

	FPAL[0]=OBJPAL[sprofs[3]&7][0];
	FPAL[1]=OBJPAL[sprofs[3]&7][1];
	FPAL[2]=OBJPAL[sprofs[3]&7][2];
	FPAL[3]=OBJPAL[sprofs[3]&7][3];

	if (!doublesize) {
	  if (tilepix>>14)
	    ((ulong *)lcdbuffer)[y+x]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((ulong *)lcdbuffer)[y+x+1]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((ulong *)lcdbuffer)[y+x+2]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((ulong *)lcdbuffer)[y+x+3]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((ulong *)lcdbuffer)[y+x+4]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((ulong *)lcdbuffer)[y+x+5]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((ulong *)lcdbuffer)[y+x+6]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((ulong *)lcdbuffer)[y+x+7]=FPAL[tilepix&0x03];
	} else {
	  if (tilepix>>14)
	    ((ulong *)lcdbuffer)[y+(x<<1)  ]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+1]=FPAL[tilepix >> 14];
	  if ((tilepix>>6)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+2]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+3]=FPAL[(tilepix >> 6)&0x03];
	  if ((tilepix>>12)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+4]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+5]=FPAL[(tilepix >> 12)&0x03];
	  if ((tilepix>>4)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+6]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+7]=FPAL[(tilepix >> 4)&0x03];
	  if ((tilepix>>10)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+8]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+9]=FPAL[(tilepix >> 10)&0x03];
	  if ((tilepix>>2)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+10]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+11]=FPAL[(tilepix >> 2)&0x03];
	  if ((tilepix>>8)&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+12]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+13]=FPAL[(tilepix >> 8)&0x03];
	  if (tilepix&0x03)
	    ((ulong *)lcdbuffer)[y+(x<<1)+14]=
	      ((ulong *)lcdbuffer)[y+(x<<1)+15]=FPAL[tilepix&0x03];
	}	
      }
    }


  }
}

void storescanline(void)
{
#ifdef HARDDEBUG
  fprintf(OUTSTREAM,"Drawing scanline %d (LCDC: %02X).\n",LY,LCDC);
#endif

  if ((LCDC & 0x80)==0) {
    memset(lcdbuffer+(uint)LY*GB_XBUFFERSIZE*(bitmapbits>>3),
	   0,GB_XBUFFERSIZE*(bitmapbits>>3));
    return;
  }

  (*scanline)();
}

/* horizontal HDMA processing */
void hdma_update(void)
{
  int i;

  if (hdmastarted) {
#ifdef HARDDEBUG
    printf("Horizontal blank transfer.\n");
#endif 

    for (i=16;i>0;i--,hdmadst++,hdmasrc++) 
      vram[VBK&1][hdmadst&0x1FFF]=GetByteAt(hdmasrc);
    if (((HDMA5&0x7F)==0)||(hdmastarted<0)) {
      hdmastarted=0;
      HDMA5=0x80;
    } else
      /*  HDMA5=(HDMA5&0x80)|((HDMA5&0x7F)-1);*/
      HDMA5--;
  }
  HDMA1=hdmasrc>>8;
  HDMA2=hdmasrc&0xF0;
  HDMA3=(hdmadst>>8)&0x1F;
  HDMA4=hdmadst&0xF0;
}

void gameboyspecifics(void)
{
#ifdef DIALOGLINK
  int n;
#endif

  /* global ticks */
  Z80_CPUCLKS+=Z80_TICKS;

  /* divider update */
  dividerclks-=Z80_TICKS;
  if (dividerclks<0) {
    dividerclks+=GB_DIVLOAD;
    DIV++;
  }

  soundclks-=Z80_TICKS;
  if (soundclks<0) {
    soundclks+=snd_updateclks;
#ifdef SOUND
    if (usingsound) processSound();
#endif
  }

#ifdef DIALOGLINK
  serialioclks-=Z80_TICKS;
  if (serialioclks<0) {
    if (colormode)
      serialioclks=serialclks[(SC&2)>>1][DBLSPEED];
    else serialioclks=serialclks[0][0];

    if ((n=dlglink_getbyte())>=0) {
      if ((SC&0x01)==0) dlglink_sndbyte(siosendout);
      SB=n;
#ifdef HARDDEBUG
      printf("Got: %02X\n",SB);
#endif
      SC&=0x7F;
      IF|=0x08;
    } 

  }
#endif

  /* ******************** */
#ifndef SOUND
  soundcounter-=Z80_TICKS;
  if (soundcounter<0) NR52&=0xF7;
#endif
  /* ******************** */

  /* video update */

  lcdclks-=Z80_TICKS;
  if (LCDC & 0x80) { /* if lcd on */
    if (lcdclks<0) {
      lcdphase++;
      if (lcdphase==3) {
	if ((LY>142)&&(LY<153)) {
	  lcdclks=GB_VCLKS[3];lcdphase--;
	  hdma_update();
	  if ((STAT&3)!=1) { /* v-blank */
	    STAT=(STAT&0xFC)|1;vblankoccured=1;
	    vblankdelay=GB_VBLANKDELAYCLKS;
	  } else {
	    if (LY==148) drawscreen();
	  }
	} else {
	  lcdphase=0;STAT=(STAT&0xFC)|2;
	  lcdclks=GB_VCLKS[0];
	  if ((IE&2)&&(STAT&0x20)) IF|=2; /* lcd oam */
	  hdma_update();
	}
	LY=LY==153 ? 0 : LY+1;
	
	STAT&=0xFB;
	STAT|=(((LYC==0)&&(LY==153))||(LY==LYC)) ? 0x04 : 0;

	if (/*(IE&2)&&*/(STAT&0x40))
	  if (((LYC!=0)&&(LY==LYC))||((LYC==0)&&(LY==153))) IF|=2;
	                                       /* lcd y coincidence */
	                                       /* & double null coincidence */
      } else {
	if (LY<144) { 
	  lcdclks=GB_VCLKS[lcdphase];
	  if (lcdphase==1) STAT=(STAT&0xFC)|3; 
	  else { 
	    STAT&=0xFC;
	    storescanline();
	    if ((IE&2)&&(STAT&0x08)) IF|=2; /* lcd h-blank */
	  }
	}
      }
    }
  } else {
    if (lcdclks<0) {
      lcdphase=lcdphase==2 ? 0 : lcdphase+1;
      lcdclks=GB_VCLKS[lcdphase];
      STAT&=0xFC;
      switch (lcdphase) {
      case 0:STAT|=2;break;
      case 1:STAT|=3;storescanline();break;
      }
    }
  }

  /* v-blank interrupt handler */
  if (vblankdelay<0) {
    if ((vblankoccured)||(IF&1)) {
      IF|=STAT & 0x10 ? 3 : 1;vblankoccured=0;
      if ((Z80_IE)&&(IE&1)) {
	Z80_IE=0;Z80_HALTED=0;IF&=0xFE;
#if 0
	for (soundclks=0x10;soundclks<0x27;soundclks++)
	  printf("%02X,",io[soundclks]);
	printf("\n");
#endif


#ifdef HARDDEBUG
	fprintf(OUTSTREAM,"V-blank interrupt.\n");
#endif
	M_RST(0x40);/* lcdclks-=16;*/
      }
    } 
  } else vblankdelay-=Z80_TICKS;

  if ((Z80_IE)&&(IF&2)&&(IE&2)) {
    Z80_IE=0;Z80_HALTED=0;IF&=0xFD;
#ifdef HARDDEBUG
	fprintf(OUTSTREAM,"LCDSTAT interrupt.\n");
#endif
	M_RST(0x48);/* lcdclks-=24;*/
  }

  /* timer interrupt handler */
  if (TAC&0x04) {
    timerclks-=Z80_TICKS;
    if (timerclks<0) {
      timerclks+=timerfreq;
      TIMA++;
      if (TIMA==0) {
	TIMA=TMA;IF|=0x04;
      }
    }
  }
  if (Z80_IE && (IE&4) && (IF&4)) {
    Z80_IE=0;IF&=0xFB;Z80_HALTED=0;
#ifdef HARDDEBUG
    fprintf(OUTSTREAM,"Timer interrupt.\n");
#endif
    M_RST(0x50);
  }

  /* serial interrupt handler */
  if (IF&0x08) {
#ifndef DIALOGLINK
      SC&=0x7F;SB=0xFF;
#endif
      if (Z80_IE && (IE&0x08)) {
	IF&=0xF7;
	Z80_IE=0;Z80_HALTED=0;
#ifdef HARDDEBUG
	fprintf(OUTSTREAM,"Serial interrupt.\n");
#endif
	M_RST(0x58);
      }
  }


  /* joypad interrupt handler (lowest priority) */

  joypadclks-=Z80_TICKS;
  if (joypadclks<0) {
    joypadclks=GB_JOYCLKS;
    joypad();
    if (joypadstate!=newjoypadstate || IF&0x10) {
      IF|=0x10;joypadstate=newjoypadstate;
      if ((IE&0x10)&&(Z80_IE)) {
	Z80_IE=0;Z80_HALTED=0;
	IF&=0xEF;
#ifdef HARDDEBUG
	fprintf(OUTSTREAM,"Joypad interrupt.\n");
#endif
	M_RST(0x60);
      }
    }
  }
}

