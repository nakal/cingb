
/* 

   dos.c

*/

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <dpmi.h>
#include <pc.h>
#include <sys/farptr.h>
#include <go32.h>

#include "globals.h"
#include "sys.h"
#include "gameboy.h"
#include "joypad.h"

short video = 0;
short keyb = 0;
unsigned long int GB_STDPAL[4];
int resolution=8;
int doublesize=0;
_go32_dpmi_seginfo keybinfo;
int exitnow=0;

static long int keystat=0;

void vramdump(tilescreen,dataofs)
int tilescreen;
int dataofs;
{
  tilescreen=0;
  dataofs=0;
  return;
}

void drawscreen(void) {
  int x,y;

  int buffer=((200-GB_LCDYSCREENSIZE)/2)*320+(320-GB_LCDXSCREENSIZE)/2;
  char *lcdpos=lcdbuffer+resolution;

  while ((inp(0x3DA)&8)>0);
  WHILE ((inp(0x3DA)&8)==0);

  for (y=0;y<GB_LCDYSCREENSIZE;y++,buffer+=320,lcdpos+=24)
    for (x=0;x<GB_LCDXSCREENSIZE;x++,lcdpos++) {
      _farpokeb(video,buffer+x,*lcdpos);
    }

}

void newkeybintr(void) {
  unsigned char c;

  keystat=c=inp(0x60);
  if (c<128) {
    /* keyboard make code */

    switch (c) {
    case GBPAD_up:
      newjoypadstate&=0xBF;
      break;
    case GBPAD_down:
      newjoypadstate&=0x7F;
      break;
    case GBPAD_left:
      newjoypadstate&=0xDF;
      break;
    case GBPAD_right:
      newjoypadstate&=0xEF;
      break;
    case GBPAD_a:
      newjoypadstate&=0xFE;
      break;
    case GBPAD_b:
      newjoypadstate&=0xFD;
      break;
    case GBPAD_start:
      newjoypadstate&=0xF7;
      break;
    case GBPAD_select:
      newjoypadstate&=0xFB;
      break;
    case 16:
    case 1:
      exitnow=1;
      break;
    }
  } else {
    /* keyboard break code */

    switch (c&0x7F) {
    case GBPAD_up:
      newjoypadstate|=0x40;
      break;
    case GBPAD_down:
      newjoypadstate|=0x80;
      break;
    case GBPAD_left:
      newjoypadstate|=0x20;
      break;
    case GBPAD_right:
      newjoypadstate|=0x10;
      break;
    case GBPAD_a:
      newjoypadstate|=0x01;
      break;
    case GBPAD_b:
      newjoypadstate|=0x02;
      break;
    case GBPAD_start:
      newjoypadstate|=0x08;
      break;
    case GBPAD_select:
      newjoypadstate|=0x04;
      break;
    }
  }

  /* keyboard buffer clear */
  _farpokeb(keyb,0x1A,_farpeekb(keyb,0x1C));
}

void joypad(void) {
  /*  printf("%04X (%0d)\n",(short int)newjoypadstate,(short int)keystat);*/

  if (exitnow) {
    savestate();
    tidyup();
    exit(0);
  }
}

int initsys(void) {
  union REGS r;
  int i;
  _go32_dpmi_seginfo newintr;

  _go32_dpmi_get_protected_mode_interrupt_vector(0x09,&keybinfo);

  newintr.pm_offset=(int)newkeybintr;
  newintr.pm_selector=_my_cs();
  _go32_dpmi_chain_protected_mode_interrupt_vector(0x09,&newintr);

  fprintf(OUTSTREAM,"Allocating lcd buffer (%d bytes)... "
	  ,GB_XBUFFERSIZE*GB_LCDYSCREENSIZE*(resolution/8));
  lcdbuffer=malloc(GB_XBUFFERSIZE*GB_LCDYSCREENSIZE*(resolution/8));
  if (lcdbuffer==NULL) {
    out_failed;
    return 1;
  } else {
    out_ok;
  }

  r.x.ax=0x0013;
  int86(0x10,&r,&r);

  video=__dpmi_segment_to_descriptor(0xa000);
  keyb=__dpmi_segment_to_descriptor(0x0040);

  GB_STDPAL[0]=color_translate(0x7FFF);
  GB_STDPAL[1]=color_translate(0x56B5);
  GB_STDPAL[2]=color_translate(0x2D6B);
  GB_STDPAL[3]=color_translate(0x0000);
  
  for (i=0;i<64;i++) {
    outp(0x3C8,i);
    outp(0x3C9,(i>>4)<<4);
    outp(0x3C9,((i>>2)&3)<<4);
    outp(0x3C9,(i&3)<<4);
  }

  return 0;
}

void donesys(void) {
  union REGS r;

  _go32_dpmi_set_protected_mode_interrupt_vector(0x09,&keybinfo);

  r.x.ax=0x0003;
  int86(0x10,&r,&r);
}

ulong color_translate(uint gbcol)
{
  return (((gbcol&0x1F)>>3)<<4)+((((gbcol>>5)&0x1F)>>3)<<2)+
    (((gbcol>>10)&0x1F)>>3);
}
