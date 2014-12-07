
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#define OUTSTREAM stdout
#include "settings.h"

char joy_device[256];

int joy_left=300;
int joy_right=800;
int joy_top=300;
int joy_bottom=800;

int joy_buttonA=0x0000;
int joy_buttonB=0x0000;
int joy_buttonSTART=0x0000;
int joy_buttonSELECT=0x0000;

#define JOY_DEV    0
#define JOY_LEFT   1
#define JOY_RIGHT  2
#define JOY_TOP    3
#define JOY_BOTTOM 4
#define JOY_BTNA   5
#define JOY_BTNB   6
#define JOY_BTNSTART  7
#define JOY_BTNSELECT 8

char *settingskeys[]={
  "joy_dev",
  "joy_left",
  "joy_right",
  "joy_top",
  "joy_bottom",
  "joy_buttonA",
  "joy_buttonB",
  "joy_buttonSTART",
  "joy_buttonSELECT",
  NULL
};

int settings_getconffilename(char *cfgname,int cfglen) {

  char *homeenv;
  
  homeenv=getenv("HOME");
  if (homeenv==NULL) return -1;
#ifdef UNIX
  strncpy(cfgname,homeenv,
	  cfglen-strlen(SETTINGS_FILE)-5);
  strcat(cfgname,"/");
#else
  strcpy(cfgname,"\\");
#endif
  strcat(cfgname,SETTINGS_FILE);

  return 0;
}

int settings_read(void) {

  FILE *setf;
  char *key,*value;
  char buffer[1024];
  char settingsfilename[512];

  joy_device[0]=0;

  if (settings_getconffilename(settingsfilename,sizeof(settingsfilename))<0) {
    
    fprintf(OUTSTREAM,"Cannot find user's home.\n");
    return 0;
  }

  fprintf(OUTSTREAM,"Opening settings in file %s ... ",settingsfilename);
  setf=fopen(settingsfilename,"r");
  if (setf==NULL) {
    out_failed;
    fprintf(OUTSTREAM,"*!* Please run 'cingb_conf', if you need "
	    "joystick support.\n");
    return 0;
  }
  out_ok;

  fprintf(OUTSTREAM,"Reading settings ... ");
  
  while (!feof(setf)) {

    if (fgets(buffer,sizeof(buffer),setf)==NULL) {
      break;
    }
    settings_trunccomment(buffer);
    if (settings_getkeyandvalue(buffer,&key,&value)==0) {
      /* got valid key+value combination */

      /*fprintf(OUTSTREAM,"'%s' = '%s'\n",key,value);*/

      switch (settings_getlistindex(settingskeys,key)) {

      case JOY_DEV:
	strncpy(joy_device,value,sizeof(joy_device));
	break;
      case JOY_LEFT:
	joy_left=atoi(value);
	break;
      case JOY_RIGHT:
	joy_right=atoi(value);
	break;
      case JOY_TOP:
	joy_top=atoi(value);
	break;
      case JOY_BOTTOM:
	joy_bottom=atoi(value);
	break;
      case JOY_BTNA:
	joy_buttonA=atoi(value);
	/*	printf("a=%d\n",joy_buttonA);*/
	break;
      case JOY_BTNB:
	joy_buttonB=atoi(value);
	/*	printf("b=%d\n",joy_buttonB);*/
	break;
      case JOY_BTNSTART:
	joy_buttonSTART=atoi(value);
	/*	printf("st=%d\n",joy_buttonSTART);*/
	break;
      case JOY_BTNSELECT:
	joy_buttonSELECT=atoi(value);
	/*	printf("se=%d\n",joy_buttonSELECT);*/
	break;
      default:
	out_failed;
	fprintf(OUTSTREAM,"Error in configuration near key:\n\t%s\n",key);
	return -1;
      }
    }
  }

  if (ferror(setf)) {
    out_failed;
    return -1;
  }

  out_ok;
  return 0;
}

void settings_trunccomment(char *line) {

  char *p;

  assert(line!=NULL,"line==NULL in settings_trunccomment");

  p=strchr(line,'#');
  if (p==NULL) return;

  *p=0;
}

int settings_getkeyandvalue(char *line,char **key,char **val) {

  settings_trimline(line);
  if (strlen(line)==0) return -1;

  *key=line;

  *val=strchr(*key,'=');

  if (*val==NULL) {
    fprintf(OUTSTREAM,"Error in configuration file near '%s'\n",line);
    return -1;
  }

  *((*val)++)=0;
  settings_trimline(*key);
  settings_trimline(*val);

  return 0;
}

void settings_trimline(char *line) {
  
  char *p;
  int idx;

  assert(line!=NULL,"line==NULL in settings_trimline");

  /*  fprintf(stderr,"trim('%s')\n",line);*/

  p=line;
  idx=0;

  while (*p<=' ' && *p>0) {
    p++;
    idx++;
  }

  if (idx>0) memmove(line,p,(strlen(line)+1)-idx);

  idx=strlen(line)-1;
  while (idx>=0) {
    if (p[idx]>' ') break;
    p[idx--]=0;
  }
}

int settings_getlistindex(char *list[],char *searchkey) {

  int i=0;

  while (list[i]!=NULL) {
    if (!strcmp(list[i],searchkey)) return i;
    i++;
  }

  return UNKNOWN_KEY;
}

/*
  DEBUG routine
void main(void) {
  settings_read();
}
*/
