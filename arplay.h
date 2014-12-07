
/*
  actionreplay.h
*/

typedef struct {
  unsigned int addr;
  int test,set;
} Ar_Code;

extern int ar_enabled;

/* prototypes */

void ar_setcode(char *code);
int ar_checkwrite(unsigned int,unsigned char);
