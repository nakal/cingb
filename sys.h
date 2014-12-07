
/* 

   sys.h

*/


extern int resolution,bitmapbits;
extern unsigned long int GB_STDPAL[4]; /* new - replacing XPAL */
extern int doublesize;
extern int usingsound;

void vramdump(int,int);
void drawscreen(void);
void joypad(void);
int initsys(void);
void donesys(void);
ulong color_translate(uint);
#ifdef DIALOGLINK
int dlglink_getbyte(void);
void dlglink_sndbyte(int);
#endif
