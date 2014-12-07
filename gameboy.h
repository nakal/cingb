
/* 

   gameboy.h
   
*/

#include "globals.h"

#define GB_ROMBANKSIZE 0x4000          /* size of a rom bank              */
#define GB_MAXROMBANKCOUNT 0x1000      /* max count of rom banks (4 MB!)  */
#define GB_MAXRAMBANKCOUNT 0x04        /* max count of ram banks (32 kB)  */
#define GB_VRAMSIZE 0x2000             /* size of the video ram           */
#define GB_RAMBANKSIZE 0x2000          /* size of a ram bank              */
#define GB_WORKBANKCOUNT 8             /* gbc work ram bank count         */
#define GB_OAMSIZE 0x100               /* size of sprite attr mem
                                          (only 0xA0 used!)               */
#define GB_IOAREASIZE 0x0100           /* io ports and free ram */
#define GB_WORKRAMBANKSIZE 0x1000      /* size of internal ram (w/o echo) */
#define GB_LCDXSCREENSIZE 160          /* lcd x size                      */
#define GB_LCDYSCREENSIZE 144          /* lcd y size                      */
#define GB_XBUFFERSIZE 184             /* buffer for scanlines 
					  (+24 worstcase?)                */
#define GB_SPRITECOUNT 40              /* maximal sprite amount           */
#define GB_DIVLOAD 256                 /* 16384 Hz                        */
#define GB_JOYCLKS 65111               /* joypad update ticks             */
#define GB_VBLANKDELAYCLKS 24          /* v-blank delay clks              */

typedef uchar *rombank;
typedef uchar *rambank;

extern int ABORT_EMULATION;
extern int snd_updateclks;
extern int soundclks;
extern int dialoglink;

extern uchar *vram[2];
extern uchar *io;
extern char *lcdbuffer;   
 
extern uint  rombanknr;
extern uchar rambanknr;
extern uchar newjoypadstate;

/* IO registers (0xFFnn) */
#define P1      io[0x00] 
#define SB      io[0x01]
#define SC      io[0x02]
#define DIV     io[0x04]
#define TIMA    io[0x05]
#define TMA     io[0x06]
#define TAC     io[0x07]
#define IF      io[0x0F]
#define LCDC    io[0x40]
#define STAT    io[0x41]
#define SCY     io[0x42]
#define SCX     io[0x43]
#define LY      io[0x44]
#define LYC     io[0x45]
#define DMA     io[0x46]
#define BGP     io[0x47]
#define OBP0    io[0x48]
#define OBP1    io[0x49]
#define WY      io[0x4A]
#define WX      io[0x4B]
#define IE      io[0xFF]


/* new CGB registers */
#define KEY1    io[0x4D]
#define VBK     io[0x4F]
#define HDMA1   io[0x51]
#define HDMA2   io[0x52]
#define HDMA3   io[0x53]
#define HDMA4   io[0x54]
#define HDMA5   io[0x55]
#define RP      io[0x56]
#define BCPS    io[0x68]
#define BCPD    io[0x69]
#define OCPS    io[0x6A]
#define OCPD    io[0x6B]
#define SVBK    io[0x70]

/* sound registers */
#define NR10    io[0x10]
#define NR11    io[0x11]
#define NR12    io[0x12]
#define NR13    io[0x13]
#define NR14    io[0x14]
#define NR21    io[0x16]
#define NR22    io[0x17]
#define NR23    io[0x18]
#define NR24    io[0x19]
#define NR30    io[0x1A]
#define NR31    io[0x1B]
#define NR32    io[0x1C]
#define NR33    io[0x1D]
#define NR34    io[0x1E]
#define NR41    io[0x20]
#define NR42    io[0x21]
#define NR43    io[0x22]
#define NR44    io[0x23]

#define NR50    io[0x24]
#define NR51    io[0x25]
#define NR52    io[0x26]


/* externals */

uchar GetByteAt(uint);
uint  GetWordAt(uint);
void SetByteAt(uint,uchar);
void SetWordAt(uint,uint);
void gameboyspecifics(void);
void savestate(void);
int initcart(char *);
void tidyup(void);
