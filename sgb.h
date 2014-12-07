

#define SGB_BUFFERSIZE    (128*8+1)
#define SGB_CMDCOUNT      32
#define SGB_CMD           (sgb_buffer[0]>>3)
#define SGB_PKGCOUNT      (sgb_buffer[0]&7)

extern int sgb_packagetransfer,sgb_byte,sgb_bitnr,sgb_bit,
  sgb_pkgdone,sgb_sgbcheck,sgb_mode,sgb_pkgnr;
extern unsigned char sgb_buffer[SGB_BUFFERSIZE];

void sgb_dumppackagebuffer(void);
void sgb_processcmd(void);
void sgb_closecmd(void);

