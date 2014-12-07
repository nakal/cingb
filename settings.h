
#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#define SETTINGS_FILE ".cingb"

#define KEY_LENGTH 32
#define VALUE_LENGTH 256

#define UNKNOWN_KEY -1

extern int  settings_read(void);
extern void settings_trunccomment(char *line);
extern int  settings_getkeyandvalue(char *line,char **key,char **val);
extern void settings_trimline(char *line);
extern int  settings_getlistindex(char *list[],char *searchkey);
extern int  settings_getconffilename(char *cfgname,int cfglen);

extern char joy_device[];

extern int joy_left;
extern int joy_right;
extern int joy_top;
extern int joy_bottom;

extern int joy_buttonA;
extern int joy_buttonB;
extern int joy_buttonSTART;
extern int joy_buttonSELECT;

#endif
