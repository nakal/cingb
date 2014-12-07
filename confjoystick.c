
/*

  confjoystick.c

  This file helps to configure your joypad/-stick buttons.

*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "settings.h"

#ifndef _LINUX
#include <sys/joystick.h>
#include <sys/uio.h>
#else
#include <linux/joystick.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef _LINUX
#define JOYPAD_DEV "/dev/js0"
#else
#define JOYPAD_DEV "/dev/joy0"
#endif

int joypaddev;
int min_x;
int min_y;
int max_x;
int max_y;

#ifdef _LINUX
unsigned int read_status(void)
{
  struct JS_DATA_TYPE js;
  
  do read(joypaddev,&js,JS_RETURN); while (js.buttons>0);
  while (js.buttons==0) read(joypaddev,&js,JS_RETURN);
  return js.buttons;
}
void read_coords(void)
{
  struct JS_DATA_TYPE js;
  
  max_x=-65535;
  max_y=-65535;
  min_x=65535;
  min_y=65535;

  do read(joypaddev,&js,JS_RETURN); while (js.buttons>0);
  do {
    read(joypaddev,&js,JS_RETURN);
    if (js.x<min_x) min_x=js.x;
    if (js.x>max_x) max_x=js.x;
    if (js.y<min_y) min_y=js.y;
    if (js.y>max_y) max_y=js.y;

    printf("X min:%d, max:%d ; Y min:%d, max:%d                    \r",
	   min_x,max_x,min_y,max_y);

  } while (js.buttons==0);
}
#else
unsigned int read_status(void)
{
  struct joystick js;
  
  do read(joypaddev,&js,sizeof(struct joystick)); while (js.b1!=0 || js.b2!=0);
  while (js.b1==0 && js.b2==0) read(joypaddev,&js,sizeof(struct joystick));
  return (js.b1&0xFF)|((js.b2&0xFF)<<8);
}
void read_coords(void)
{
  struct joystick js;
  
  max_x=-65535;
  max_y=-65535;
  min_x=65535;
  min_y=65535;

  do read(joypaddev,&js,sizeof(struct joystick)); while (js.b1!=0 || js.b2!=0);
  do {
    read(joypaddev,&js,sizeof(struct joystick));
    if (js.x<min_x) min_x=js.x;
    if (js.x>max_x) max_x=js.x;
    if (js.y<min_y) min_y=js.y;
    if (js.y>max_y) max_y=js.y;

    printf("X min:%d, max:%d ; Y min:%d, max:%d                    \r",
	   min_x,max_x,min_y,max_y);

  } while (js.b1==0 && js.b2==0);
}
#endif

int main(void)
{
#ifdef _LINUX
  struct JS_DATA_SAVE_TYPE_32 jsd;
  int status;
#endif
  unsigned int keyA=0x0001,keyB=0x0002,
               keySTART=0x0100,keySELECT=0x0200;
  char joy_device[128];
  FILE *cfgfile;
  char cfgfilename[512];

  int joy_left=-1,joy_right=-1,joy_top=-1,joy_bottom=-1;

  if (settings_getconffilename(cfgfilename,sizeof(cfgfilename))<0) {
    
    printf("Cannot find user's home.\n");
    return 0;
  }

  printf("Joypad configuration started ... ok\n");

  printf("Enter the joypad device (e.g. \""JOYPAD_DEV"\"): ");
  scanf("%s",joy_device);
  printf("Opening joypad device ... ");
  joypaddev=open(joy_device,O_RDONLY);

  if (joypaddev<0) {
    strcpy(joy_device,JOYPAD_DEV);
    printf("FAILED\n");
  } else {
    printf("ok\n");
#ifdef _LINUX
    printf("Reading joystick info ... ");
    status=ioctl(joypaddev,JS_GET_ALL,&jsd);
    if (status<0) {
      strcpy(joy_device,JOYPAD_DEV);
      printf("FAILED\n");
      close(joypaddev);
    } else { 
#endif
      printf("ok\n");
      printf("-------------------------------------------------\n");
      printf("Now you must press the proper keys on your joypad\n");
      printf("-------------------------------------------------\n");

      printf("Press GAMEBOY-A-key on your joypad.\n");
      keyA=read_status();
      printf("Press GAMEBOY-B-key on your joypad.\n");
      keyB=read_status();
#ifdef _LINUX
      printf("Press GAMEBOY-START-key on your joypad.\n");
      keySTART=read_status();
      printf("Press GAMEBOY-SELECT-key on your joypad.\n");
      keySELECT=read_status();
#else
      keySTART=0;
      keySELECT=0;
#endif

      printf("-------------------------------------------------\n");
      printf("Calibration: Please move the joypad in every\n");
      printf("direction and press a button.\n");
      printf("-------------------------------------------------\n");

      read_coords();

      if (min_x==-65535 || min_y==-65535 ||
	  max_x==65535 || max_y==65535) {
	printf("\nERROR: Calibration is NOT precise enough!\n");
      }

      joy_right=joy_left=(min_x+max_x)/2;
      joy_left-=(max_x-min_x)/5;
      joy_right+=(max_x-min_x)/5;
      joy_top=joy_bottom=(min_y+max_y)/2;
      joy_top-=(max_y-min_y)/5;
      joy_bottom+=(max_y-min_y)/5;

#ifdef _LINUX
    }
#endif
  }

  printf("\nWriting %s ... ",cfgfilename);
  if ((cfgfile=fopen(cfgfilename,"w"))!=NULL) {
    fprintf(cfgfile,"\n#   This file was generated by 'cingb_conf'");
    fprintf(cfgfile,"#   it contains settings for cingb\n\n\n");

    fprintf(cfgfile,"joy_dev=%s\n",joy_device);
    fprintf(cfgfile,"joy_left=%d\n",joy_left);
    fprintf(cfgfile,"joy_right=%d\n",joy_right);
    fprintf(cfgfile,"joy_top=%d\n",joy_top);
    fprintf(cfgfile,"joy_bottom=%d\n\n",joy_bottom);

    fprintf(cfgfile,"joy_buttonA=%d\n",keyA);
    fprintf(cfgfile,"joy_buttonB=%d\n",keyB);
    fprintf(cfgfile,"joy_buttonSTART=%d\n",keySTART);
    fprintf(cfgfile,"joy_buttonSELECT=%d\n",keySELECT);
    fclose(cfgfile);
    printf("ok.\n\n");
  } else 
    printf("FAILED.\n");
  
  exit(0);
}