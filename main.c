
/* 

   main.c

   This file contains the execution start-point.

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <getopt.h>
#else
#include <windows.h>
#endif

#include "globals.h"
#include "gameboy.h"
#include "z80.h"
#include "arplay.h"
#include "sound.h"
#include "settings.h"

FILE *logfile;
int smallview=1,producesound=1,force_stdgameboy=0, check_xvideo=1;

#ifndef _WIN32
int main(int,char **);
#endif

char servername[64];

/* online help */

void outhelp(char *execname)
{
  

  printf("\nSyntax:     %s [options] romfile\n\n",execname);
#ifndef GLIDE
  printf("Options:    -d           execute in double-size mode\n");
#endif
#ifdef WITH_XVIDEO 
  printf("            -x           don't use XVideo extension\n");
#endif
#ifdef SOUND
  printf("            -n           sound off\n");
  printf("            -f freq      sound frequency (8000-44100)\n");
#endif
  printf("            -o           force old standard gameboy mode\n");
#ifdef DIALOGLINK
  printf("            -c server    connect to server (dialog link)\n");
  printf("            -h           start as server for dialog link\n");
#endif
  printf("            -a code      use action-replay code\n");     

  printf("\n");
  exit(-1);
}

/* execution start-point */

#ifndef _WIN32
int main(argc,argv)
int argc;
char **argv;
#else
WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR lpCmdLine,int nShowCmd)
#endif
{

#ifndef _WIN32
  int check=0x00000001;
  char *chk;
#endif

  int c;

#ifndef _WIN32
  if ((sizeof(uchar)!=1)||(sizeof(uint)!=2)||(sizeof(ulong)!=4)) {
    printf("Fixme: Compilation failed.\n");
    return 1;
  }

  chk=(char *)(&check)+3;
#ifdef USE_LITTLE_ENDIAN
  if (*chk!=0) {
#else
  if (*chk!=1) {
#endif
    printf("Compilation failed.\n");
#ifdef USE_LITTLE_ENDIAN
    printf("Remove -DUSE_LITTLE_ENDIAN from the Makefile.\n");
#else
    printf("Add -DUSE_LITTLE_ENDIAN to the Makefile.\n");
#endif
    printf("Did you run ./configure ?\n");
    return 1;
  }

  while ((c=getopt(argc,argv,"vdxonhc:a:f:"))!=EOF) {
    switch (c) {
    case 'v':
      printf("cingb: version %s\n",VERSION_STRING);
      exit(0);
      break;
#ifdef WITH_XVIDEO      
    case 'x':
      check_xvideo=0;
      break;
#endif
    case 'd':
      smallview=0;
      break;
#ifdef SOUND
    case 'n':
      producesound=0;
      break;
    case 'f':
      sound_frequency=atoi(optarg);
      if (sound_frequency<8000 || sound_frequency>48000) {
	fprintf(OUTSTREAM,"Sound frequency must be in range 8000-44100.\n");
	exit(1);
      }
      break;
#endif
    case 'o':
      force_stdgameboy=1;
      break;
#ifdef DIALOGLINK
    case 'h':
      dialoglink=1;
      servername[0]=0;
      break;
    case 'c':
      dialoglink=1;
      if (strlen(optarg)>=sizeof(servername)) {
	fprintf(OUTSTREAM,"Server name too long.\n");
	exit(1);
      }
      strcpy(servername,optarg);
      break;
#endif
    case 'a':
      ar_setcode(optarg);
      break;
    default:
      outhelp(argv[0]);
      break;
    }
  }
  
  c=argc-1;
  while ((argv[c][0]=='-')&&(c>0)) c--;
  if (c==0) outhelp(argv[0]);

#else

  /* windows settings */
  smallview=0;
  producesound=0;
  force_stdgameboy=0;
#ifdef DIALOGLINK
  dialoglink=0;
#endif
c=1;

#endif


 if (settings_read()<0) exit(-1);

  /*
  logfile=fopen("cingb.log","w");
  */
  logfile=stdout;

#ifdef _WIN32
  if (!initcart("test.gb")) {
#else
  if (!initcart(argv[c])) {
#endif
    
    /* starting rom emulation */
    StartCPU();
  } else {
#ifndef _WIN32
    printf("Couldn't start %s.\n",argv[c]);
#else
	MessageBox(NULL,"Could not start the cartridge.","Error",MB_OK+MB_ICONSTOP);
#endif
    tidyup();
    return 1;
  }

  tidyup();
  return 0;
}

