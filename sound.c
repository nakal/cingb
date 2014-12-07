
/*
  sound.c

  Gameboy sound emulation support for Linux.

  *****************************************************
  EXPERIMENTAL !!!!!
  SOUNDS TERRIBLE !!!!!!!!
  *****************************************************

  */


#ifdef SOUND

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#ifndef AUDIO
#include <sys/soundcard.h>
#else
#include <sys/audioio.h>
#endif

#include "globals.h"
#include "gameboy.h"


int sd;                       /* dsp/audio device descriptor */

int gain;
int snd_count1,snd_count2,snd_count3,snd_count4;
int snd_reload1,snd_init1,snd_length1,snd_count1,snd1_st,snd1_str;
int snd_reload2,snd_init2,snd_length2,snd_count2;
int snd_reload3,snd_init3,snd_length3,snd_count3,snd_shr;
int snd_reload4,snd_init4,snd_length4,snd4_rand;

#define FIXPOINT_SHIFT 16
#define FIXPOINT_MASK  ((1L<<FIXPOINT_SHIFT)-1)
#define FIXPOINT_HALF  (1L<<(FIXPOINT_SHIFT-1))

/* current sound rate (set it higher, if you dare) */

int sound_frequency=22050;
int frequency_div256;

char *dspdev="/dev/dsp";
char *audiodev="/dev/audio";

int *pattern[4];
int chn3_tab[16];   /*={-32,-28,-24,-20,-16,-12,-8,-4,
		     0,4,8,12,16,20,24,28}*/

#define SND4_RNDPATTERNLEN 16384
int snd4_rndpattern[SND4_RNDPATTERNLEN];

int gbfreq(unsigned short int);
void outsample(int s);
/*void* processSound(void *);*/


/* sound initialisation */

int initsound(void)
{
  /*  pthread_t tid;*/
  int value;

#ifdef AUDIO
  audio_info_t audio;
#endif

#ifndef AUDIO
  if ((sd=open(dspdev,O_WRONLY))<0) {
    fprintf(OUTSTREAM,"Error opening %s\n",dspdev);
    return 0;
  }

  value=AFMT_U8;
  if (ioctl(sd,SNDCTL_DSP_SETFMT,&value)<0) {
    fprintf(OUTSTREAM,"Error: SNDCLT_DSP_SETFMT\n");
    close(sd);
    return 0;
  }
  
  value=sound_frequency;
  if (ioctl(sd,SNDCTL_DSP_SPEED,&value)<0) {
    fprintf(OUTSTREAM,"Error: SNDCLT_DSP_SPEED\n");
    close(sd);
    return 0;
  }

  value=0x0003000a;  /* 3 bufs, 2^8 bytes */
  if (ioctl(sd,SNDCTL_DSP_SETFRAGMENT,&value)<0) {
    fprintf(OUTSTREAM,"Error: SNDCLT_DSP_SETFRAGMENT\n");
    close(sd);
    return 0;
  }
#else
  if ((sd=open(audiodev,O_WRONLY))<0) {
    fprintf(OUTSTREAM,"Error opening %s\n",audiodev);
    close(sd);
    return 0;
  }

  AUDIO_INITINFO (&audio);
  audio.play.sample_rate = sound_frequency;
  audio.play.channels = 1;
  audio.play.precision = 8;
  audio.play.encoding = AUDIO_ENCODING_LINEAR;
  
  if (ioctl (sd, AUDIO_SETINFO, &audio)<0) {
    fprintf(OUTSTREAM,"Failed to set %s.\n",audiodev);
    return 0;
  }
  if (ioctl (sd, AUDIO_GETINFO, &audio) != 0) return (0);
#endif

  fprintf(OUTSTREAM,"Allocating sound pattern memory 4x%lu bytes.\n",
	  (long unsigned int)sound_frequency*sizeof(int));
  for (value=0;value<4;value++) {
    if ((pattern[value]=malloc(sound_frequency*sizeof(int)))==NULL) {
      fprintf(OUTSTREAM,"Error allocating sound pattern memory.\n");
      close(sd);
      return 0;
    }
  }

  snd_updateclks=(4096*1024)/sound_frequency;

  snd_init1=snd_length1=snd_count1=0;
  snd_init2=snd_length2=snd_count2=0;
  snd_init3=snd_length3=snd_count3=0;
  snd_init4=snd_length4=snd_count4=0;

  fprintf(OUTSTREAM,"Initializing sound pattern memory.\n");
  for (value=0;value<sound_frequency;value++) {
    /*    pattern[2][value]=sin((float)value/DSP_FREQ*2*M_PI)*128+128;
    if (value>=DSP_FREQ/2)
      pattern[1][value]=sin((float)value/DSP_FREQ*4*M_PI)*128+128;
    else pattern[1][value]=0;
    if (value>=DSP_FREQ/4)
      pattern[0][value]=sin((float)value/DSP_FREQ*8*M_PI)*128+128;
    else pattern[0][value]=0;
    if (value>=3*DSP_FREQ/4)
      pattern[3][value]=sin((float)value/DSP_FREQ*1.5*M_PI)*128+128;
    else pattern[3][value]=0;
            pattern[value]=255-((float)value/DSP_FREQ*255.0);*/
    if (value>=sound_frequency/2) pattern[2][value]=128-32; 
    else pattern[2][value]=128+32;
    if (value>=sound_frequency/4) pattern[1][value]=128-32; 
    else pattern[1][value]=128+32;
    if (value>=sound_frequency/8) pattern[0][value]=128-32; 
    else pattern[0][value]=128+32;
    if (value>=sound_frequency/4) pattern[3][value]=128+32; 
    else pattern[3][value]=128-32;
  }

  srand(time(NULL));
  for (value=0;value<SND4_RNDPATTERNLEN;value++) {

    snd4_rndpattern[value]=(rand()>>26)+128-32;
    if (value<16) {
      snd4_rndpattern[value]*=(value/16.0);
    }
      /*      printf("%d\n",snd4_rndpattern[value]);*/
  }

  for (value=0;value<16;value++) 
    chn3_tab[value]=(value-8)*4+128;

  fprintf(OUTSTREAM,"Sound pattern memory OK.\n");

  frequency_div256=sound_frequency/256;

  /*  value=pthread_create(&tid,NULL,processSound,NULL);
  fprintf(OUTSTREAM,"Sound thread initialized %s.\n", value==0 ? "OK" : "FAILED");
  if (value!=0) {
    fprintf(OUTSTREAM,"Failed with value %d.\n",value);
    }*/

  return 1; /* sound will be emulated */
}

/* closing sound */
void donesound(void)
{
  int value;

  for (value=0;value<4;value++) free(pattern[value]);
  if (sd>0) close(sd);
}

#if 0
#ifdef USE_LITTLE_ENDIAN
#define SND_BUFSIZE 1024
#define SND_BUFCOUNT 3
static int snd_bufptr=0;
static int snd_bufnr=0;
static char snd_buffer[SND_BUFCOUNT][SND_BUFSIZE];
#endif
#endif

void outsample(int s)
{
#if 0
#ifdef USE_LITTLE_ENDIAN
  
  snd_buffer[snd_bufnr][snd_bufptr++]=(char)s;

  if (snd_bufptr>=SND_BUFSIZE) {
    int playbuf=snd_bufnr-1;
    if (playbuf<0) playbuf=SND_BUFCOUNT-1;

    write(sd,snd_buffer[playbuf],SND_BUFSIZE);
    snd_bufnr++;
    snd_bufptr=0;
    if (snd_bufnr>=SND_BUFCOUNT) snd_bufnr=0;
  }
#else
#endif
#endif
  char lf;
  lf=(char)s;

  write(sd,&lf,1);

  /*      printf("%d",s);*/
}

int mixsample(int s1,int s2)
{
  register int s;

  s=((s1-128)+(s2-128))+128;

  return s<=0 ? 0 :
    s>=255 ? 255 : s;
}

void processSound(void)
{

  /*  while (1) {*/

    int sample=128,gain=128,pos;

    /*    usleep(1);*/

    /*printf("THREAD\n");*/
    /*while (Z80_CPUCLKS & 0x03);*/

    if (((NR50&0x77)==0)||((NR52&0x80)==0)) {
      if (producesound) outsample(sample);
      else outsample(0);
    }

#if 1
  if (NR51&0x11 && NR12&0xF8) {
    if (snd_init1) {
      snd_init1=0;
      snd_count1=frequency_div256;
    }
    
    if ((snd_reload1>0)&&(snd_length1>0)) {
      if (snd_count1--<=0) {
	snd_length1--;
	snd_count1=frequency_div256;

	/* sweep */
	if (snd1_str>0) {
	  if (--snd1_st==0) {
	    snd1_st=snd1_str;
	    
	    pos=((NR14&7)<<8)|NR13;
	    if (NR10&8) 
	      pos-=pos>>(NR10&7);
	    else
	      pos+=pos>>(NR10&7);

	    NR14=(NR14&0xF8)|((pos>>8)&7);
	    NR13=pos&0xFF;

	    snd_reload1=gbfreq(pos);
	  }
	}
      }
      
      snd_count1+=snd_reload1;
      /*      if (snd_count1>=sound_frequency) snd_count1-=sound_frequency;
	      gain=pattern[NR11>>6][snd_count1];*/

      gain=((snd_count1&FIXPOINT_MASK)>FIXPOINT_HALF) ? 128-32 : 128+32;

      sample=gain;
    } else { 
      gain=128;
      NR52&=0xFE;
    }
  }
#endif

#if 1
  if (NR51&0x22) {
    if (snd_init2) {
      snd_init2=0;
      snd_count2=frequency_div256;
    }
    
    if ((snd_reload2>0)&&(snd_length2>0)) {
      if (snd_count2--<=0) {
	snd_length2--;
	snd_count2=frequency_div256;
      }
      
      snd_count2+=snd_reload2;
      /*      if (snd_count2>=sound_frequency) snd_count2-=sound_frequency;
	      gain=pattern[NR21>>6][snd_count2];*/

      gain=((snd_count2&FIXPOINT_MASK)>FIXPOINT_HALF) ? 128-32 : 128+32;

      sample=mixsample(gain,sample);
    } else { 
      gain=128;
      NR52&=0xFD;
    }
  }
#endif

#if 1
  if (NR51&0x44 && NR30&0x80 && NR32&60) {
    if (snd_init3) {
      snd_init3=0;
      snd_count3=frequency_div256;
    }
    
    if ((snd_reload3>0)&&(snd_length3>0)) {
      if (snd_count3--<=0) {
	snd_length3--;
	snd_count3=frequency_div256;
      }
      
      snd_count3+=snd_reload3;
      /*      if (snd_count3>=sound_frequency) snd_count3-=sound_frequency;*/
      
      pos=((snd_count3<<5)>>FIXPOINT_SHIFT)&0x1F;

      if (pos&1) gain=chn3_tab[(io[0x30+(pos>>1)]&0x0F)>>snd_shr];
      else gain=chn3_tab[(io[0x30+(pos>>1)]>>4)>>snd_shr];

      sample=mixsample(gain,sample);
    } else { 
      gain=128;
      NR52&=0xFB;
    }
  }
#endif

#if 1
  if (NR51&0x88) {
    if (snd_init4) {
      snd_init4=0;
      snd_count4=frequency_div256;
    }
    
    if (snd_length4>0) {
      /*      snd_reload4=0;*/
      if (snd_count4--<=0) {
	snd_length4--;
	snd_count4=frequency_div256;
      }
      /*            printf("sound4 (i:%d,l:%d,c:%d)\n",snd_init4,snd_length4,snd_count4);*/

      if (snd4_rand>=SND4_RNDPATTERNLEN) snd4_rand=0;
      gain=snd4_rndpattern[snd4_rand++];
      
      /*      snd4_rand=snd4_rand*0x111163+0x3211234;
	      gain=rand()>>24;*/

      /* printf("sample: %d\n",gain);*/

      sample=mixsample(gain,sample);
    } else { 
      gain=128;
      NR52&=0xF7;
    }
  }
#endif

     if (producesound) outsample(sample);
     else outsample(128);
     /*  }*/
}

int gbfreq(unsigned short int value)
{
  /*int freq;*/

  /* printf("%09f\n",(float)((131072<<13)/((2048L-value)<<13))); */

  return value>0 ? ((((131072<<13)/(2048L-value))<<3)/sound_frequency) : 0;

  /*  freq=value>0 ?  131072/(2048-value) : 0;
  if (freq>=sound_frequency/2) freq=0;
  return freq;*/
}

#endif
