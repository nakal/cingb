
/*

  sound.h
  
*/

extern int snd_length1;
extern int snd_init1;
extern int snd_reload1;
extern int snd1_st;
extern int snd1_str;
extern int snd_length2;
extern int snd_init2;
extern int snd_reload2;
extern int snd_length3;
extern int snd_init3;
extern int snd_reload3;
extern int snd_shr;
extern int snd_reload4;
extern int snd_length4;
extern int snd_init4;

extern int sound_frequency;

int  initsound(void);
void donesound(void);
int  gbfreq(unsigned short int);
void processSound(void);
