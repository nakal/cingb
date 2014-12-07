/*

  unix.c

  This file contains system specific information
  (e.g. displaying routines, joystick access,
        window controls etc ...)

*/

#ifdef UNIX

#ifdef GLIDE
#ifdef FRAMEBUFFER
#error: Do not try to make GLIDE & FRAMEBUFFER !!!
#endif
#if !defined JOYSTICK && !defined BSD_JOYSTICK
#error: Do not try to make GLIDE w/o JOYSTICK/BSD_JOYSTICK !!!
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "globals.h"
#include "sys.h"
#include "gameboy.h"
#include "z80.h"
#include "arplay.h"
#include "settings.h"

/* unix signal processing */
#define __USE_POSIX
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef GLIDE
/* X specific */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef MIT_SHM
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#endif

#ifdef WITH_XVIDEO
#include <X11/extensions/Xvlib.h>
#endif

#endif

#ifdef JOYSTICK
/* linux joystick device */
#include <linux/joystick.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#endif
#ifdef BSD_JOYSTICK
/* BSD joystick device */
#include <sys/joystick.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

#ifdef FRAMEBUFFER
/* framebuffer device */
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <linux/vt.h>

char *fbdevname="/dev/fb0";
struct fb_fix_screeninfo fb_finfo;
struct fb_var_screeninfo fb_vinfo,fb_vinfo_orig;
struct termios termsave;
struct vt_mode vt_modesave;

int fbdev,tty,fbgb_linesize,ttystate;
int usingfb;
char *fbmem,*fbgb;
#endif

#ifdef GLIDE
#include <glide.h>
#include <sst1vid.h>
#include <termios.h>

struct termios termsave;
#endif

#ifdef DIALOGLINK
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

char *sock_tcpsignature="cingb___init___socket";
int dialoglink,dlglink_status,sock_d,sock_nd,sock_desc;

int dlglink_init(void);
#endif

/* experimental ***************************************/
#ifdef SOUND
#include "sound.h"
#endif

#include "joypad.h"

#ifndef GLIDE
char *gbwintitle="cingb";

Display *display;
Visual *visual;
Window gbwin;
Colormap colormap;
int screen;
XColor *colors;
static XImage *gameboyscreen;
GC gbgc;
static size_t lcdbuffersize=0;

#ifdef MIT_SHM
/* XShm */
static Status use_xshm;
static XShmSegmentInfo shminfo;
#endif

#ifdef WITH_XVIDEO
static int use_xvideo=0;
static unsigned int xv_port=0, xv_format;
static XvImage *xv_gameboyscreen;
static int win_width=GB_LCDXSCREENSIZE;
static int win_height=GB_LCDYSCREENSIZE;
#endif
#endif

int joypaddev,usejoypad,usekeys,timerset,
  resolution,doublesize,dsupdatenow,usingsound,bitmapbits;
volatile int refreshtimer;
struct itimerval itv;
int REDSHL,GREENSHL,BLUESHL;
int REDOFS,GREENOFS,BLUEOFS;

#ifdef JOYSTICK
struct JS_DATA_TYPE js;
#endif
#ifdef BSD_JOYSTICK
struct joystick js;
#endif

uchar GB_STD8BITPAL[]=
{
  215,
  172,
  86,
  0
};

unsigned long int GB_STDPAL[4];


void timerhandler(int signr)
{
#ifdef _LINUX
  signal(SIGALRM, timerhandler);
#endif
  refreshtimer++;
}

#ifdef FRAMEBUFFER

void FB_ClearScreen(void)
{
  memset(fbmem,0,fb_vinfo.xres*fb_vinfo.yres*resolution/8);
}

void tty_switchhandler(int signal)
{
  switch (signal) {
  case SIGUSR1:
    ttystate=0;
    ioctl(tty, VT_RELDISP, 1);
    break;
  case SIGUSR2:
    ttystate=1;
    ioctl(tty, VT_RELDISP, VT_ACKACQ);
    FB_ClearScreen();
    break;
  }
}

int InitFB(void)
{
  struct termios term;
  struct vt_mode vtm;
  struct sigaction old,now;

  fbdev=open(fbdevname,O_RDWR,0);
  if (!fbdev) {
    fprintf(stderr,"Could not open %s or an X connection.\n",fbdevname);
    return 1;
  }
  if (ioctl(fbdev,FBIOGET_FSCREENINFO,&fb_finfo)<0) {
    fprintf(stderr,"Error: FBIOGET_FSCREENINFO\n");
    return 1;
  }
  if (ioctl(fbdev,FBIOGET_VSCREENINFO,&fb_vinfo)<0) {
    fprintf(stderr,"Error: FBIOGET_VSCREENINFO\n");
    return 1;
  }
  bitmapbits=resolution=fb_vinfo.bits_per_pixel;

  if (resolution<16) {
    fprintf(stderr,"Sorry: I don't support %d bits resolution, yet.\n",
	    resolution);
    return 1;
  }
  memcpy(&fb_vinfo_orig,&fb_vinfo,sizeof(fb_vinfo_orig));

  fbmem=mmap(NULL,fb_finfo.smem_len,PROT_WRITE,
	     MAP_SHARED,fbdev,0);

  if (fbmem==MAP_FAILED) {
    fprintf(stderr,"Error: mmap failed.\n");
    return 1;
  }

  if (!smallview) {
    fbgb=fbmem+(fb_vinfo.xres-GB_LCDXSCREENSIZE*2)*resolution/16+
      (fb_vinfo.yres-GB_LCDYSCREENSIZE*2)/2*fb_finfo.line_length;
    fbgb_linesize=GB_LCDXSCREENSIZE*2*resolution/8;
  } else {
    fbgb=fbmem+(fb_vinfo.xres-GB_LCDXSCREENSIZE)*resolution/16+
      (fb_vinfo.yres-GB_LCDYSCREENSIZE)/2*fb_finfo.line_length;
    fbgb_linesize=GB_LCDXSCREENSIZE*resolution/8;
  }


  if (fb_vinfo.xoffset!=0 || fb_vinfo.yoffset!=0) {
    fb_vinfo.xoffset=0;
    fb_vinfo.yoffset=0;
    if (ioctl(fbdev,FBIOPAN_DISPLAY,&fb_vinfo)<0) {
      fprintf(OUTSTREAM,"Error: FBIOPAN_DISPLAY\n");
      return 1;
    }
  }

  usingfb=1;
  FB_ClearScreen();

  logfile=fopen("cingb.log","w");
  if (logfile==NULL) {
    fprintf(stdout,"Could not open logfile (using stdout).\n");
    logfile=stdout;
  }

#ifdef DEBUG  
  printf("FB_TYPE  : %d\n",fb_finfo.type);
  printf("FB_VISUAL: %d\n",fb_finfo.visual); 
  printf("Resolution: %dx%dx%d\n",fb_vinfo.xres,fb_vinfo.yres,
	 fb_vinfo.bits_per_pixel);
  printf("Pixel format: red  : %d-%d\n",fb_vinfo.red.offset,
	 fb_vinfo.red.offset+fb_vinfo.red.length-1);
  printf("              green: %d-%d\n",fb_vinfo.green.offset,
	 fb_vinfo.green.offset+fb_vinfo.green.length-1);
  printf("              blue : %d-%d\n",fb_vinfo.blue.offset,
	 fb_vinfo.blue.offset+fb_vinfo.blue.length-1);
#endif
  REDOFS  =fb_vinfo.red.offset;
  GREENOFS=fb_vinfo.green.offset;
  BLUEOFS =fb_vinfo.blue.offset;

  REDSHL  =fb_vinfo.red.length-5;
  GREENSHL=fb_vinfo.green.length-5;
  BLUESHL =fb_vinfo.blue.length-5;

  /* init terminal settings */
  fprintf(OUTSTREAM,"Setting up terminal ... ");
  tcgetattr(0,&termsave);
  memcpy(&term,&termsave,sizeof(struct termios));
  cfmakeraw(&term);
  term.c_iflag|=BRKINT;
  term.c_lflag|=ISIG;
  tcsetattr(0,TCSANOW,&term);
  out_ok;

  /* tty init */
  fprintf(OUTSTREAM,"Setting up tty device ... ");
  if ((tty=open("/dev/tty",O_RDWR))<0) {
    fprintf(OUTSTREAM,"Error opening tty device.\n");
    return 1;
  }
  if (ioctl(tty,VT_GETMODE,&vt_modesave)<0) {
    fprintf(OUTSTREAM,"Error writing to tty device.\n");
    return 1;
  }
  memcpy(&vtm,&vt_modesave,sizeof(struct vt_mode));
  now.sa_handler  = tty_switchhandler;
  now.sa_flags    = 0;
  now.sa_restorer = NULL;
  sigemptyset(&now.sa_mask);
  sigaction(SIGUSR1,&now,&old);
  sigaction(SIGUSR2,&now,&old);

  vtm.mode   = VT_PROCESS;
  vtm.waitv  = 0;
  vtm.relsig = SIGUSR1;
  vtm.acqsig = SIGUSR2;

  if (ioctl(tty,VT_SETMODE,&vtm)<0) {
    fprintf(OUTSTREAM,"Error writing to tty device.\n");
    return 1;
  }
  ttystate=1;
  out_ok;

  return 0;
}

void DoneFB(void)
{
  /* clean up */

  ioctl(tty,VT_SETMODE,&vt_modesave);
  close(tty);

  tcsetattr(0,TCSANOW,&termsave);
  FB_ClearScreen();

  if (ioctl(fbdev,FBIOPUT_VSCREENINFO,&fb_vinfo_orig)<0) {
    fprintf(stderr,"Error: FBIOPUT_VSCREENINFO. (restoring state)\n");
  }
  
  if (munmap(fbmem,fb_finfo.smem_len)) {
    fprintf(stderr,"Error: munmap failed.\n");
  }
  close(fbdev);
#ifdef VERBOSE
  fprintf(OUTSTREAM,"Framebuffer device closed.\n");
#endif
  fclose(logfile);
  logfile=stdout;
}
#endif

void allocate_lcdbuffer(void) {
	lcdbuffersize=doublesize ?
		GB_XBUFFERSIZE*GB_LCDYSCREENSIZE*(bitmapbits/2) :
		GB_XBUFFERSIZE*GB_LCDYSCREENSIZE*(bitmapbits/8);
	fprintf(OUTSTREAM,"Allocating lcd buffer (%lu bytes%s)... ",
		(size_t)lcdbuffersize, doublesize ? "; double-size" : "");
	lcdbuffer=malloc(lcdbuffersize);
	if (lcdbuffer==NULL) out_failed; else out_ok;
}

#ifdef GLIDE

/* **************************************** */
int initGlide(void)
{
  GrHwConfiguration hwconfig;
  struct termios term;

  /* init terminal settings */
  fprintf(OUTSTREAM,"Setting up terminal ... ");
  tcgetattr(0,&termsave);
  memcpy(&term,&termsave,sizeof(struct termios));
  cfmakeraw(&term);
  term.c_iflag|=BRKINT;
  term.c_lflag|=ISIG;
  tcsetattr(0,TCSANOW,&term);
  out_ok;

  fprintf(OUTSTREAM,"Initializing glide ... ");
  grGlideInit();
  fprintf(OUTSTREAM,"ok\nSearching glide hardware ...\n");
  if (grSstQueryHardware(&hwconfig)==FXTRUE) {
    grSstSelect(0);
    if (grSstWinOpen(0,GR_RESOLUTION_640x480,GR_REFRESH_NONE,
		     GR_COLORFORMAT_ARGB,GR_ORIGIN_UPPER_LEFT,2,1)!=FXTRUE) {
      fprintf(OUTSTREAM,
	      "*** Error: Failed to set the standard 640x480 mode.\n");
      return 1;
    }
  } else {
    fprintf(OUTSTREAM,"*** Error: glide hardware not found.\n");
    return 1;
  }

  grColorCombine( GR_COMBINE_FUNCTION_LOCAL,
		  GR_COMBINE_FACTOR_NONE,
		  GR_COMBINE_LOCAL_CONSTANT,
		  GR_COMBINE_OTHER_NONE,
		  FXFALSE );

  grBufferClear(0,0,0);
  grBufferSwap(0);
  grBufferClear(0,0,0);
  grDisableAllEffects();

  /* fixed sst1 format (there aren't others defined) */

  bitmapbits=resolution=16;
  REDSHL=BLUESHL=0;
  GREENSHL=1;
  REDOFS=11;
  GREENOFS=5;
  BLUEOFS=0;

  out_ok;

  allocate_lcdbuffer();

  return 0;
}
void doneGlide(void)
{
  grGlideShutdown();
  tcsetattr(0,TCSANOW,&termsave);
}

#else
int InitXConnection(void)
{
  XVisualInfo visualinfo;
  XVisualInfo *retvinfo;
  char *displayname=NULL;
  int i,v;
#ifdef MIT_SHM
  int major,minor;
  Bool shared;
#endif
#ifdef WITH_XVIDEO
  unsigned int xv_ver, xv_rel, xv_reqbase, xv_eventbase, xv_errbase,
  	adaptorcount;
  XvAdaptorInfo *adaptorinfo = NULL;
#endif

  display=XOpenDisplay(displayname);

  if (display==NULL)
    {
#if defined(FRAMEBUFFER) && !defined(DEBUG)
      return InitFB();
#else
      fprintf(stderr,"Cannot connect to X-Server.\n");
      return 1;
#endif
    }

#ifdef WITH_XVIDEO

	if (check_xvideo) {
		fprintf(OUTSTREAM, "Checking for XVideo extension ... ");
		use_xvideo=(XvQueryExtension(display, &xv_ver, &xv_rel,
		&xv_reqbase, &xv_eventbase, &xv_errbase)==Success);
		if (!use_xvideo) out_failed;
		else {
			out_ok;
			fprintf(OUTSTREAM, "Fetching XVideo adapters ... ");
			if (XvQueryAdaptors(display,DefaultRootWindow(display),
				&adaptorcount,&adaptorinfo)!=Success){

				use_xvideo=0;
				out_failed;
			} else out_ok;
		}
	} else {
		use_xvideo=0;
	}

	if (use_xvideo) {
		int i;
		int inuse=0;

		xv_port=0;
		fprintf(OUTSTREAM, "Opening XVideo port (adapters: %d) ... ",
			adaptorcount);

		for (i=0; i<adaptorcount && xv_port==0; i++) {
			if ((adaptorinfo[i].type&XvInputMask) &&
				(adaptorinfo[i].type&XvImageMask)) {
				
				XvPortID id;

				for (id=adaptorinfo[i].base_id;
					id<adaptorinfo[i].base_id+
					adaptorinfo[i].num_ports; id++)

					if (XvGrabPort(display, id,
						CurrentTime)==Success) {

						xv_port = id;
						break;
					} inuse=1;
			}
		}

		if (xv_port==0) {
			if (inuse) fprintf(OUTSTREAM, "all ports in use ");
			out_failed;
			use_xvideo=0;
		} else {
			int i;
			int formats=0;
			XvImageFormatValues *fo;

			fo = XvListImageFormats(display, xv_port, (int*)&formats);

			xv_format=0;
			for(i=0; i<formats; i++){
				fprintf(OUTSTREAM, "Available XVideo image format: 0x%x (%4.4s) %s\n", fo[i].id,(char*)&fo[i].id, (fo[i].format == XvPacked) ? "packed" : "planar");
				if (xv_format==0 &&
				(fo[i].id==0x59565955|| fo[i].id==0x55595659)) {
					xv_format = fo[i].id;
					fprintf(OUTSTREAM, "y:%d, u:%d, v:%d (bits/pixel:%d)\n",
					  fo[i].y_sample_bits,
					  fo[i].u_sample_bits,
					  fo[i].v_sample_bits, fo[i].bits_per_pixel);
				}
			}
			fprintf(OUTSTREAM, "Selecting XVideo output format ... ");
			if (xv_format==0) {
				XvUngrabPort(display, xv_port, CurrentTime);
				use_xvideo=0;
				out_failed;
			} else out_ok;
		}
	}
#endif

#ifdef MIT_SHM
  use_xshm=XShmQueryVersion (display, &major, &minor, &shared) && shared;
#endif
  

#ifdef FRAMEBUFFER
  usingfb=0;
#endif

  if (!smallview) doublesize=1;

  screen=DefaultScreen(display);
  visual=DefaultVisual(display, screen);
  visualinfo.visualid=visual->visualid;
  retvinfo=XGetVisualInfo(display, VisualIDMask, &visualinfo, &i);
  if (retvinfo==NULL) {
	  fprintf(stderr, "Error getting VisualInfo for visual id %u.\n",
			  (unsigned int)visual->visualid);
	  exit(-1);
  }
  memcpy(&visualinfo, retvinfo, sizeof(XVisualInfo));
  XFree(retvinfo);

  bitmapbits=BitmapUnit(display);

	if (use_xvideo) {
		bitmapbits=32;
		fprintf(OUTSTREAM,"Screen bitmap bits is %i, resolution is %i bits (YUV).\n", bitmapbits, resolution);
	} else {
		resolution = visualinfo.depth;
		fprintf(OUTSTREAM,"Screen  bitmap bits is %i, resolution is %i bits.\n", bitmapbits, resolution);
	}

  if (bitmapbits==8) {
    /* create a palette with 6 shades for every color
       this will be slower (multiplication) but it
       looks far better than 4 shades.                 */

      colormap=XCreateColormap(display, RootWindow(display,screen),
			       visual,AllocAll);
      colors=(XColor *)calloc(256,sizeof(XColor));
      for (i=0;i<256;i++)
	{
	  colors[i].pixel=i;
	  colors[i].flags=DoRed|DoGreen|DoBlue;

	  if (i<216)
	    {
	      v=(i / 36) % 6;
	      colors[i].red=v*13107;
	      v=(i / 6) % 6;
	      colors[i].green=v*13107;
	      v=i % 6;
	      colors[i].blue=v*13107;
	    }
	}
      XStoreColors(display,colormap,colors,256);
  } else {
    REDOFS=BLUEOFS=GREENOFS=0;
    REDSHL=BLUESHL=GREENSHL=0;
    for (i=visualinfo.red_mask;!(i&1);i>>=1,REDOFS++);
    for (;i&1;i>>=1,REDSHL++);
    for (i=visualinfo.green_mask;!(i&1);i>>=1,GREENOFS++);
    for (;i&1;i>>=1,GREENSHL++);
    for (i=visualinfo.blue_mask;!(i&1);i>>=1,BLUEOFS++);
    for (;i&1;i>>=1,BLUESHL++);
    REDSHL-=5;GREENSHL-=5;BLUESHL-=5;
  }
  return 0;
}
#endif

/* signal hookfunc */
void DoBreak(int signum)
{
  if (!ABORT_EMULATION) {
    ABORT_EMULATION=1;
    savestate();
    tidyup();
    exit(0);
  }
}

#ifdef MIT_SHM
int xerrorhandler (Display *display, XErrorEvent *event) {
    use_xshm=0;
    return 0;
}
#endif


#ifndef GLIDE
int InitXRessources(void)
{
  XSetWindowAttributes gbwinattr;
  XTextProperty prop_gbwintitle;
  XGCValues values;
#ifdef MIT_SHM
  XErrorHandler old_handler;
#endif


  fprintf(OUTSTREAM,"Window initialization ... ");

  gbwinattr.event_mask=StructureNotifyMask|ExposureMask|KeyPressMask|
  	KeyReleaseMask;
  gbwinattr.colormap=colormap;
  gbwinattr.border_pixel=0;

  gbwin=XCreateWindow(display,
		      RootWindow(display,screen),
		      100,50,
		      smallview ? GB_LCDXSCREENSIZE : GB_LCDXSCREENSIZE*2,
		      smallview ? GB_LCDYSCREENSIZE : GB_LCDYSCREENSIZE*2,
		      0,resolution,
		      InputOutput,
			    visual,
		      CWEventMask|
		      CWColormap|
		      CWBorderPixel,
		      &gbwinattr);

  if (!gbwin) {
    out_failed;
    return 1;
  } else out_ok;
  
  values.background=0;
  gbgc=XCreateGC(display,gbwin,GCBackground,&values);
  
  XStringListToTextProperty(&gbwintitle,1,&prop_gbwintitle);
  XSetWMName(display,gbwin,&prop_gbwintitle);
  /*XSelectInput(display,gbwin,ExposureMask|KeyPressMask|KeyReleaseMask);*/
  XMapRaised(display,gbwin);
  XFlush(display);

  dsupdatenow=0;


  fprintf(OUTSTREAM,"Display initialized.\nCreating display image buffer ... ");

#ifdef MIT_SHM
 tryagain:
  if (use_xshm) {

#ifdef WITH_XVIDEO
	if (use_xvideo) {
		fprintf(OUTSTREAM,"(xv+shared) ");
		xv_gameboyscreen=(XvImage *)XvShmCreateImage(display,
			xv_port, xv_format, lcdbuffer,
			GB_XBUFFERSIZE*2, GB_LCDYSCREENSIZE, &shminfo);
		if (xv_gameboyscreen==NULL) {
			fprintf(OUTSTREAM,"screen allocation failed for XVideo"
				" trying X11\n Creating image buffer ... ");
			use_xvideo=0;
			goto tryagain;
		}
	}
	else
#endif	
	{
	    fprintf(OUTSTREAM,"(shared) ");
		gameboyscreen=XShmCreateImage(display,visual,resolution,
			ZPixmap,NULL,&shminfo,
			smallview ? GB_XBUFFERSIZE : GB_XBUFFERSIZE*2,
			smallview ? GB_LCDYSCREENSIZE : GB_LCDYSCREENSIZE*2);
	}

    if (gameboyscreen==NULL) {
      use_xshm=0;
      out_failed;
      fprintf(OUTSTREAM,"Trying normal mode ...\n");
      goto tryagain;
    }

    shminfo.shmid=shmget(IPC_PRIVATE,gameboyscreen->bytes_per_line*
			 gameboyscreen->height,
			 IPC_CREAT|0777);

    if (shminfo.shmid<0) {
      out_failed;
      fprintf(OUTSTREAM,"Trying normal mode ...\n");
      XDestroyImage(gameboyscreen);
      use_xshm=0;
      goto tryagain;
    }

    lcdbuffer=shminfo.shmaddr=gameboyscreen->data=shmat(shminfo.shmid,0,0);

    if (!lcdbuffer) {
      out_failed;
      fprintf(OUTSTREAM,"Trying normal mode ...\n");
      XDestroyImage(gameboyscreen);
      shmctl(shminfo.shmid,IPC_RMID,0);
      use_xshm=0;
      goto tryagain;
    }

    shminfo.readOnly=False;
    old_handler=XSetErrorHandler(xerrorhandler);
    XShmAttach(display,&shminfo);
    XSync(display,False);
    XShmPutImage(display,gbwin,gbgc,gameboyscreen,8,0,0,0,
		 1,1,0);
    XSync(display,False);
    XSetErrorHandler (old_handler);

    if (!use_xshm) {
      out_failed;
      fprintf(OUTSTREAM,"Trying normal mode ...\n");
      XDestroyImage(gameboyscreen);
      shmdt(shminfo.shmaddr);
      shmctl(shminfo.shmid,IPC_RMID,0);
      goto tryagain;
    } else fprintf(OUTSTREAM,"shared OK\n");
  } else {

#endif

	allocate_lcdbuffer();
	gameboyscreen=NULL;

#ifdef WITH_XVIDEO
	xv_gameboyscreen=NULL;
	if (use_xvideo) {
		fprintf(OUTSTREAM,"(xv) ");
		xv_gameboyscreen=(XvImage *)XvCreateImage(display,
			xv_port, xv_format, lcdbuffer,
			GB_XBUFFERSIZE*2, GB_LCDYSCREENSIZE);

		if (xv_gameboyscreen==NULL) {
			fprintf(OUTSTREAM,"screen allocation failed for XVideo"
				" trying X11\n Creating image buffer ... ");
			use_xvideo=0;
			free(lcdbuffer);
			goto tryagain;
		}

	} else
#endif	
	{
		fprintf(OUTSTREAM,"(x11) ");
		gameboyscreen=XCreateImage(display,visual,resolution,ZPixmap,
			0, lcdbuffer,
			smallview ? GB_XBUFFERSIZE : GB_XBUFFERSIZE*2,
			smallview ? GB_LCDYSCREENSIZE : GB_LCDYSCREENSIZE*2,
			bitmapbits,0);
	}

#ifdef MIT_SHM
  }
#endif


  fprintf(OUTSTREAM,"Preparing image ... ");
  if (!use_xvideo) {
	  fprintf(OUTSTREAM,"X11 ... ");
	  if (gameboyscreen) out_ok; else {
		  out_failed;
		  return 1;
	  }
  } else {
	  fprintf(OUTSTREAM,"XVideo (using %d bytes) ... ",
	  	xv_gameboyscreen->data_size);
	  if (!xv_gameboyscreen) {
		  out_failed;
		  return 1;
	  }

	  if (xv_gameboyscreen->data_size>lcdbuffersize) {
		  out_failed;
		  return 1;
	  }

	  doublesize=0;
	  smallview=1;
	  out_ok;
  }

#ifndef DEBUG
  XAutoRepeatOff(display);
#endif
  return 0;
}
#endif

int initsys(void)
{
  int retval;
#ifdef JOYSTICK
  int status;
  struct JS_DATA_SAVE_TYPE_32 jsd;
#endif

  doublesize=0;
#if defined(GLIDE) && !defined(DEBUG)
  retval=initGlide();
#else 
  if ((retval=InitXConnection())!=0) return 1;
  if (!retval 
#ifdef FRAMEBUFFER
      && !usingfb
#endif
               ) retval=InitXRessources();
#endif

#ifdef MIT_SHM
  if (use_xshm)
#endif
     fprintf(OUTSTREAM,"Buffer allocation ... ");

  if (lcdbuffer==NULL) {
    out_failed;
    retval=1;
  } else out_ok;

  /* B/W palette initialisation */
  GB_STDPAL[0]=color_translate(0x7FFF);
  GB_STDPAL[1]=color_translate(0x56B5);
  GB_STDPAL[2]=color_translate(0x2D6B);
  GB_STDPAL[3]=color_translate(0x0000);

  usejoypad=0;
#if defined JOYSTICK || defined BSD_JOYSTICK
  fprintf(OUTSTREAM,"Opening joypad device ... ");
  joypaddev=open(joy_device,O_RDONLY);
  if (joypaddev<0) {
    out_failed;
  } else {
    out_ok;
#ifdef JOYSTICK
    fprintf(OUTSTREAM,"Reading joystick info ... ");
    status=ioctl(joypaddev,JS_GET_ALL,&jsd);
    if (status<0) {
      out_failed;
      close(joypaddev);
    } else { 
      out_ok;
      usejoypad=1; 
    }
#endif
#ifdef BSD_JOYSTICK
      usejoypad=1; 
#endif
  }
#endif

#ifdef GLIDE
  if (!usejoypad) {
    doneGlide();
    return 1;
  }
#endif

  signal(SIGHUP,DoBreak);signal(SIGINT,DoBreak);
  signal(SIGQUIT,DoBreak);signal(SIGTERM,DoBreak);
  signal(SIGPIPE,DoBreak);

#ifdef __SVR4
  sigset(SIGALRM,timerhandler);
#else
  signal(SIGALRM,timerhandler);
#endif


  itv.it_value.tv_usec=20000;
  itv.it_value.tv_sec=0;
  itv.it_interval.tv_usec=20000;
  itv.it_interval.tv_sec=0;
  timerset=setitimer(ITIMER_REAL,&itv,NULL);
  refreshtimer=0;
  if (timerset<0) {
    fprintf(OUTSTREAM,"setitimer() failed./n");
    fprintf(OUTSTREAM,"no real-time emulation will be available.\n");
  }


#ifdef SOUND
  if (producesound) {
    fprintf(OUTSTREAM,"Initialize sound emulation ... \n");
    usingsound=initsound();
    fprintf(OUTSTREAM,"Sound %s\n",usingsound ? "OK" : "FAILED");
  }
#endif

#ifdef DIALOGLINK
  if (dialoglink)
    dlglink_status=dlglink_init();
#endif

  return retval;
}

#ifndef GLIDE
void DoneXConnection(void)
{
#ifndef DEBUG
  XAutoRepeatOn(display);
#endif
  
#ifdef WITH_XVIDEO
	fprintf(OUTSTREAM, "Freeing XVideo ressources ... ");
	if (use_xvideo && xv_port) {
		XvUngrabPort(display, xv_port, CurrentTime);
	}
#endif

#ifdef VERBOSE
  fprintf(OUTSTREAM,"Destroying window and freeing memory ... ");
#endif

#ifdef MIT_SHM
  if (use_xshm) {
    XShmDetach(display,&shminfo);
  }
#endif

#ifdef WITH_XVIDEO
	if (!use_xvideo)
#endif
	  XDestroyImage(gameboyscreen);

#ifdef MIT_SHM
  if (use_xshm) {
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid,IPC_RMID,0);
  }
#endif

  XUnmapWindow(display,gbwin);
  XDestroyWindow(display,gbwin);
  XFreeGC(display,gbgc);
  if (bitmapbits==8) {
    XFreeColormap(display,colormap);
    free(colors);
  }
#ifdef VERBOSE
  out_ok;
#endif
  
#ifdef VERBOSE
  fprintf(OUTSTREAM,"Shutting down X-connection ... ");fflush(stdout);
#endif
  XCloseDisplay(display);
#ifdef VERBOSE
  out_ok;
#endif
}
#endif

void donesys(void)
{
#ifdef DIALOGLINK
  switch (dlglink_status) {
  case 1:
    close(sock_nd);
    close(sock_d);
    break;
  case 2:
    close(sock_d);
    break;
  }
#endif

#ifdef SOUND
  if (producesound) {
    fprintf(OUTSTREAM,"Closing sound.\n");
    donesound();
  }
#endif


#if defined JOYSTICK || defined BSD_JOYSTICK
#ifdef VERBOSE
  fprintf(OUTSTREAM,"Closing joypad device ... ");
#endif
  if (joypaddev>0) close(joypaddev);
  out_ok;
#endif


#ifdef GLIDE
    doneGlide();
#else 
#ifdef FRAMEBUFFER
  if (!usingfb)
#endif
    DoneXConnection();
#ifdef FRAMEBUFFER
  else DoneFB();
#endif
#endif
}

unsigned int color_translate(unsigned short int gbcol)
{
	/* hope, it works now for every type of visual */

	if (!use_xvideo) {

		if (bitmapbits>8)
			return (((gbcol&0x1F)<<REDSHL)<<REDOFS)|
				((((gbcol>>5)&0x1F)<<GREENSHL)<<GREENOFS)|
				((((gbcol>>10)&0x1F)<<BLUESHL)<<BLUEOFS);
		else 
			return ((gbcol&0x1F)/6)*36+(((gbcol>>5)&0x1F)/6)*6+
				((gbcol>>10)&0x1F)/6;
	} else {
		int r;
		int g;
		int b;

		int y, u, v;

		r=(gbcol&0x1F)<<3;
		g=((gbcol>>5)&0x1F)<<3;
		b=((gbcol>>10)&0x1F)<<3;

		y = (((66*r+129*g+25*b)>>8)+16)&0xff;
		u = (((-38*r-74*g+112*b)>>8)+128);
		v = (((112*r-94*g-18*b)>>8)+128);

		/*fprintf(stderr, "r: %d, g: %d, b: %d\n", r, g, b);
		fprintf(stderr, "y: %d, u: %d, v: %d\n", y, u, v);*/

		return (y<<24)|(y<<8)|u|(v<<16);

		/* return (v<<24)|(y<<16)|(u<<8)|y; */
		
	}
}

void drawscreen(void)
{
#ifndef GLIDE
#ifdef FRAMEBUFFER
  int y;
  char *lcdpos,*buffer;
  register int x,v;
  char *buffer2;
#endif

  if (doublesize 
#ifdef MIT_SHM
      && !use_xshm
#endif
    ) {
    if (dsupdatenow++==0) return;
    if (dsupdatenow>2) dsupdatenow=0;
  }

  if (smallview) {
#ifdef FRAMEBUFFER
    if (usingfb) {
      if (ttystate) {
	for (y=0,buffer=fbgb,lcdpos=lcdbuffer+resolution;
	     y<GB_LCDYSCREENSIZE;y++,buffer+=fb_finfo.line_length,
	       lcdpos+=GB_XBUFFERSIZE*resolution/8) {
	  memcpy(buffer,lcdpos,fbgb_linesize);
	}
      }
    } else
#endif

#ifdef MIT_SHM
      if (use_xshm) {
#ifdef WITH_XVIDEO
	      if (use_xvideo)
		      XvShmPutImage(display, xv_port, gbwin, gbgc,
		      	xv_gameboyscreen, 16, 0,
			GB_LCDXSCREENSIZE*2, GB_LCDYSCREENSIZE,
			0,0,
			win_width, win_height,
			False);
	      else
#endif
		      XShmPutImage(display,gbwin,gbgc,gameboyscreen,8,0,0,0,
				      GB_LCDXSCREENSIZE,GB_LCDYSCREENSIZE,0);
      } else 
#endif

#ifdef WITH_XVIDEO
      if (use_xvideo) {
	      XvPutImage(display, xv_port, gbwin, gbgc,
			      xv_gameboyscreen, 16, 0,
			      GB_LCDXSCREENSIZE*2, GB_LCDYSCREENSIZE,
			      0,0,
				win_width, win_height);
      } else
#endif
	XPutImage(display,gbwin,gbgc,gameboyscreen,8,0,0,0,
		  GB_LCDXSCREENSIZE,GB_LCDYSCREENSIZE);
  } else {
#ifdef FRAMEBUFFER
    if (usingfb) {
      if (ttystate) {
	for (y=0,buffer=fbgb,buffer2=fbgb+fb_finfo.line_length,
	       lcdpos=lcdbuffer+resolution;
	     y<GB_LCDYSCREENSIZE;y++,buffer+=fb_finfo.line_length<<1,
	       buffer2+=fb_finfo.line_length<<1,
	       lcdpos+=GB_XBUFFERSIZE*resolution/8) {
	  switch (resolution) {
	  case 8:
	    for (x=0;x<GB_LCDXSCREENSIZE;x++) {
	      v=((unsigned char *)lcdpos)[x];v|=v<<8;
	      ((unsigned short int *)buffer)[x]=v;
	      ((unsigned short int *)buffer2)[x]=v;
	    }
	    break;
	  case 16:
	    for (x=0;x<GB_LCDXSCREENSIZE;x++) {
	      v=((unsigned short int *)lcdpos)[x];v|=v<<16;
	      ((unsigned int *)buffer)[x]=v;
	      ((unsigned int *)buffer2)[x]=v;
	    }
	    break;
	  case 32:
	    for (x=0;x<(GB_LCDXSCREENSIZE<<1);x+=2) {
	      v=((unsigned int *)lcdpos)[x>>1];
	      ((unsigned int *)buffer)[x]=v;
	      ((unsigned int *)buffer)[x+1]=v;
	      ((unsigned int *)buffer2)[x]=v;
	      ((unsigned int *)buffer2)[x+1]=v;
	    }
	    break;
	  }
	}
      }
    } else
#endif

#ifdef MIT_SHM
      if (use_xshm) {
	      XShmPutImage(display,gbwin,gbgc,gameboyscreen,16,0,0,0,
			      GB_LCDXSCREENSIZE*2,GB_LCDYSCREENSIZE*2,
			      0);
      } else 
#endif
	      XPutImage(display,gbwin,gbgc,gameboyscreen,16,0,0,0,
			      GB_LCDXSCREENSIZE*2,GB_LCDYSCREENSIZE*2);
  }

#ifdef FRAMEBUFFER
  if (!usingfb)
#endif
    /*    XFlush(display);*/
#else
  /* Glide version of drawscreen *********************************** */

  GrLfbInfo_t info;
  unsigned short int *buffer,*buffer2,*lcdpos;
  register int x,y;
  register unsigned int v;

  if (!grLfbLock(GR_LFB_WRITE_ONLY,GR_BUFFER_BACKBUFFER,
		 GR_LFBWRITEMODE_565,GR_ORIGIN_UPPER_LEFT,
		 FXFALSE,&info)) {
    grGlideShutdown();
    printf("Lock failed.\n");
    ABORT_EMULATION=1;
    return;
  }
  buffer=info.lfbPtr+((240-GB_LCDYSCREENSIZE)*info.strideInBytes)+
    320-(GB_LCDXSCREENSIZE>>1);
  buffer2=buffer+(info.strideInBytes>>1);

  for (y=0,lcdpos=(unsigned short int *)(lcdbuffer+resolution);
       y<GB_LCDYSCREENSIZE;y++,
	 buffer+=info.strideInBytes,buffer2+=info.strideInBytes,
	 lcdpos+=GB_XBUFFERSIZE) {
    for (x=0;x<GB_LCDXSCREENSIZE;x++) {
      v=lcdpos[x];v|=v<<16;
      ((unsigned int *)buffer)[x]=v;
      ((unsigned int *)buffer2)[x]=v;
    }
  }
  
  grLfbUnlock(GR_LFB_WRITE_ONLY,GR_BUFFER_BACKBUFFER);
  grBufferSwap(1);
#endif

  if (usingsound) return;

  if (timerset>=0) {
      while (refreshtimer<1); /* sync loop */
  } else {
      while (refreshtimer<3); /* sync loop */
  }
  refreshtimer=0;
}

void joypad(void)
{
#if defined JOYSTICK || defined BSD_JOYSTICK
  int status;
#endif
#if defined(FRAMEBUFFER) || defined (GLIDE)
  int retval;
  fd_set rfds;
  struct timeval tv;
#endif
#ifndef GLIDE
  XEvent E;

#ifdef FRAMEBUFFER
  if (!usingfb) {
#endif
  while (XCheckWindowEvent(display,gbwin,KeyPressMask,&E)) {
    usekeys=1;
    switch(XLookupKeysym((XKeyEvent *)&E,0)) {
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
    case XK_Escape:
    case XK_q:
      usekeys=0;
      DoBreak(0);
      break;
    case XK_Return:
      ar_enabled=ar_enabled ? 0 : 1;
      break;
#ifdef DEBUG
    case XK_b:
      breakpoint=-1;
      skipover=0;
      usekeys=0;
      break;
    case XK_t:
      db_trace=db_trace ? 0 : 1;
      usekeys=0;
      break;
#endif
    }
  }

  while (XCheckWindowEvent(display,gbwin,KeyReleaseMask,&E)) {
    switch(XLookupKeysym((XKeyEvent *)&E,0)) {
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
    if (newjoypadstate==0xFF) usekeys=0;
  }

  while (XCheckWindowEvent(display, gbwin, StructureNotifyMask, &E)) {
	  if (E.type==ConfigureNotify) {
		  win_width=E.xconfigure.width;
		  win_height=E.xconfigure.height;
	  }
  }
#ifdef FRAMEBUFFER
  }
#endif 
#endif

#if defined(FRAMEBUFFER) || defined(GLIDE)
#ifdef FRAMEBUFFER
  if (usingfb) {
#endif
    FD_ZERO(&rfds);
    FD_SET(0,&rfds);
    tv.tv_sec=0;tv.tv_usec=0;
    retval=select(1,&rfds,NULL,NULL,&tv);

    if (retval) {
      read(0,&retval,1);
      /*
      printf("Pressed: %i\n",retval&0xFF);
      */

      switch (retval&0xFF) {
      case 27:
	FD_ZERO(&rfds);
	FD_SET(0,&rfds);
	tv.tv_sec=0;tv.tv_usec=0;
	retval=select(1,&rfds,NULL,NULL,&tv);
	if (retval) {
	  read(0,&retval,1);
	  read(0,&retval,1);
	  switch (retval&0xFF) {
	    /*  case 65:printf("up\n");break;
	  case 66:printf("down\n");break;
	  case 67:printf("right\n");break;
	  case 68:printf("left\n");break;*/
	  }
	} else DoBreak(0);
	break;
      case 113:
	DoBreak(0);
	break;
      case 13:
	ar_enabled=ar_enabled ? 0 : 1;
	break;
      }
    }
#ifdef FRAMEBUFFER
  }
#endif
#endif


#if defined JOYSTICK || defined BSD_JOYSTICK
  if (usejoypad && (!usekeys)) {
#ifdef JOYSTICK
    status=read(joypaddev,&js,JS_RETURN);
    if (status==JS_RETURN) {
      newjoypadstate=
	(js.x<joy_left   ? 0 : 0x20)|
	(js.x>joy_right  ? 0 : 0x10)|
	(js.y<joy_top    ? 0 : 0x40)|
	(js.y>joy_bottom ? 0 : 0x80)|
	(js.buttons & joy_buttonA      ? 0 : 0x01)|
	(js.buttons & joy_buttonB      ? 0 : 0x02)|
	(js.buttons & joy_buttonSTART  ? 0 : 0x08)|
	(js.buttons & joy_buttonSELECT ? 0 : 0x04);
    }
#else
    status=read(joypaddev,&js,sizeof(struct joystick));
    if (status==sizeof(struct joystick)) {
      int buttons;

      buttons=(js.b1&0xFF)|((js.b2&0xFF)<<8);

      newjoypadstate=
	(js.x<joy_left   ? 0 : 0x20)|
	(js.x>joy_right  ? 0 : 0x10)|
	(js.y<joy_top    ? 0 : 0x40)|
	(js.y>joy_bottom ? 0 : 0x80)|
	(buttons & joy_buttonA      ? 0 : 0x01)|
	(buttons & joy_buttonB      ? 0 : 0x02)|
	(buttons & joy_buttonSTART  ? 0 : 0x08)|
	(buttons & joy_buttonSELECT ? 0 : 0x04);
    }
#endif
  }
#endif
}

#ifndef GLIDE
void vramdump(tilescreen,dataofs)
int tilescreen;
int dataofs;
{
  char *dumpwintitle="VRAM dump";
  Window dumpwin;
  XSetWindowAttributes gbwinattr;
  XTextProperty prop_wintitle;
  XEvent E;
  int x,y,xx,yy;
  GC gc;
  XGCValues gcval;
  uchar *tileofs,*tile,*tdofs;
  uchar scan1,scan2;

#ifdef FRAMEBUFFER
  if (usingfb) return;
#endif

  gbwinattr.event_mask=ExposureMask|KeyPressMask;
  gbwinattr.colormap=colormap;
  gbwinattr.border_pixel=0;
  dumpwin=XCreateWindow(display,
		      RootWindow(display,screen),
		      300,50,256,256,
		      0,resolution,
		      InputOutput,
			    visual,
		      CWEventMask|
		      CWColormap|
		      CWBorderPixel,
		      &gbwinattr);

  if (!dumpwin) return;
  
  XStringListToTextProperty(&dumpwintitle,1,&prop_wintitle);
  XSetWMName(display,dumpwin,&prop_wintitle);
  XSelectInput(display,dumpwin,ExposureMask|KeyPressMask);
  XMapRaised(display,dumpwin);
  XWindowEvent(display,dumpwin,ExposureMask,&E);

  gcval.background=0;
  gc=XCreateGC(display,dumpwin,GCBackground,&gcval);
  
  tileofs=vram[0]+0x1800+(tilescreen ? 0x400 : 0);
  tdofs=vram[0]+(dataofs ? 0x0000 : 0x1000);
  for (y=0;y<32;y++)
    for (x=0;x<32;x++) {
      tile=tdofs+(dataofs ? (int)tileofs[y*32+x]*16 : 
	(int)((signed char)tileofs[y*32+x])*16);
      for (yy=0;yy<8;yy++) {
	scan1=(tile+yy*2)[0];
	scan2=(tile+yy*2)[1];
	for (xx=0;xx<8;xx++) {
	  gcval.foreground=GB_STDPAL[3-(
		(BGP>>
		((3-(((scan1>>(7-xx))&1)|(((scan2>>(7-xx))&1)<<1)))*2))&3)
	  ];
	  XChangeGC(display,gc,GCForeground,&gcval);
	  XDrawLine(display,dumpwin,gc,x*8+xx,y*8+yy,
		    x*8+xx,y*8+yy);
	}
      }
    }

  XFreeGC(display,gc);
  while (1) {
    XNextEvent(display,&E);
    if (E.type==KeyPress) break;
  }
  fprintf(OUTSTREAM,"exited.\n");
  XDestroyWindow(display,dumpwin);
}
#endif

#ifdef DIALOGLINK

int dlglink_getbyte(void)
{
  int c=0;
  fd_set fds;
  struct timeval tv;

  FD_ZERO(&fds);
  FD_SET(sock_desc,&fds);
  tv.tv_sec=0;tv.tv_usec=0;

  c=select(16,&fds,NULL,NULL,&tv);

  if (c<=0) return -1;

  if (recv(sock_desc,&c,1,0)<=0) return -1;

  return c;
}

void dlglink_sndbyte(int c)
{
  while (send(sock_desc,&c,1,0)<=0);
}

void dlglink_getstr(char *buf,int size)
{
  int n,ch;

  for (n=0;n<size;n++) {
    while ((ch=dlglink_getbyte())<0);
    buf[n]=ch;
  }
}

void dlglink_sndstr(char *buf,int size)
{
  int n;

  for (n=0;n<size;n++) dlglink_sndbyte(buf[n]);
}

#define TCPPORT 9121
int dlglink_init(void)
{
  struct sockaddr_in sock_name;
  int n;
  socklen_t sock_len;
  struct hostent *sock_hp;
  char buffer[64];

  if ((sock_d=socket(AF_INET,SOCK_STREAM,0))<0) {
    fprintf(OUTSTREAM,"Error: socket initialization failed.\n");
    fprintf(OUTSTREAM,"No dialog link supported.\n");
    exit(1);
  }

  sock_len=sizeof(struct sockaddr_in);

  if (strlen(servername)==0) {
    /* server side init */

    memset(&sock_name,0,sizeof(struct sockaddr_in));
    
    sock_name.sin_family=AF_INET;
    sock_name.sin_port=htons(TCPPORT);
    n=INADDR_ANY;
    memcpy(&sock_name.sin_addr,&n,sizeof(n));

    if (bind(sock_d,(struct sockaddr *)&sock_name,sock_len)<0) {
      fprintf(OUTSTREAM,"Error: bind to port %i failed.\n",TCPPORT);
      close(sock_d);
      exit(1);
    }

    if (listen(sock_d,1)<0) {
      fprintf(OUTSTREAM,"Error: listen on port %i failed.\n",TCPPORT);
      close(sock_d);
      exit(1);
    }

    fprintf(OUTSTREAM,"Waiting for the other side ... ");
    if ((sock_nd=accept(sock_d,(struct sockaddr *)&sock_name,&sock_len))<0) {
      out_failed;
      close(sock_d);
      exit(1);
    } else out_ok;


    sock_desc=sock_nd;

    dlglink_sndstr(sock_tcpsignature,strlen(sock_tcpsignature)+1);
    dlglink_getstr(buffer,strlen(sock_tcpsignature)+1);

    if (strcmp(buffer,sock_tcpsignature)) {
      fprintf(OUTSTREAM,"Not a cingb-socket ?\nReceived: %s\n",buffer);
      close(sock_d);
      exit(1);
    }

    fprintf(OUTSTREAM,"Connection established.\n");

    return 1;

  } else {
    /* client side init */

    if ((sock_hp=gethostbyname(servername))==NULL) {
      fprintf(OUTSTREAM,"Error: unknown host %s.\n",servername);
      fprintf(OUTSTREAM,"Closing emulation.\n");
      close(sock_d);
      exit(1);
    }

    memset(&sock_name,0,sizeof(struct sockaddr_in));
    
    sock_name.sin_family=AF_INET;
    sock_name.sin_port=htons(TCPPORT);
    memcpy(&sock_name.sin_addr,sock_hp->h_addr_list[0],sock_hp->h_length);

    fprintf(OUTSTREAM,"Trying to connect to server ... ");
    if (connect(sock_d,(struct sockaddr *)&sock_name,sock_len)<0) {
      out_failed;
      close(sock_d);
      exit(1);
    } else out_ok;

    sock_desc=sock_d;

    dlglink_getstr(buffer,strlen(sock_tcpsignature)+1);

    if (strcmp(buffer,sock_tcpsignature)) {
      fprintf(OUTSTREAM,"Not a cingb-socket ?\nReceived: %s\n",buffer);
      close(sock_d);
      exit(1);
    }
    dlglink_sndstr(sock_tcpsignature,strlen(sock_tcpsignature)+1);

    fprintf(OUTSTREAM,"Connection established.\n");

    return 2;
  }
}
#endif /* DIALOGLINK */

#endif /* UNIX */
