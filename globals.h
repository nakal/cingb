
/* 

   globaldefs.h

   This file contains some other crap,
   which I couldn't write somewhere else.

*/

#ifndef GLOBALS_INCLUDED
#define GLOBALS_INCLUDED

#define VERSION_STRING "0.32"

#define uchar unsigned char

#ifdef UNIX
#define uint  unsigned short int
#else
#define uint  unsigned int
#endif

#ifdef UNIX
#define ulong unsigned int
#endif
#if defined DOS || defined _WIN32
#define ulong unsigned long int
#endif

#if defined(FRAMEBUFFER) || defined(DOS)
#define OUTSTREAM logfile
#else
#define OUTSTREAM stdout
#endif

#define out_ok     fprintf(OUTSTREAM,"OK\n")
#define out_failed fprintf(OUTSTREAM,"FAILED\n")

extern char *gbwintitle;
extern FILE *logfile;
extern int smallview;
extern int check_xvideo;
extern int producesound;
extern int force_stdgameboy;
extern char servername[64];

#define assert(x,msg) \
if (!(x)) { \
    fprintf(OUTSTREAM,"\nAssertion failed in file "__FILE__ \
                      "line %d:\n\t%s\n", __LINE__,msg); \
}

#endif
