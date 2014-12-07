
/* 

   joypad.h

   This file contains the X-keyboard configuration.

*/


/* joypad configuration (keys in X-mode) */

#ifdef UNIX
#define GBPAD_up                             XK_Up
#define GBPAD_down                           XK_Down
#define GBPAD_left                           XK_Left
#define GBPAD_right                          XK_Right
#define GBPAD_a                              XK_c
#define GBPAD_b                              XK_x
#define GBPAD_start                          XK_s
#define GBPAD_select                         XK_d
#endif

#ifdef DOS
#define GBPAD_up                             72
#define GBPAD_down                           80
#define GBPAD_left                           75
#define GBPAD_right                          77
#define GBPAD_a                              46
#define GBPAD_b                              45
#define GBPAD_start                          31
#define GBPAD_select                         32
#endif
