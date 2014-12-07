/*

  conftest.c

  This file checks your system configuration to
  adjust the Makefile for cingb

*/

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  char testarray[8]={0,0,0,1,0,0,0,0};

  printf("OK cc: compilation & execution works ...\n");
  printf("OK cc: checking type-sizes ... ");
  if (sizeof(int)!=4) {
    printf("int: not 4 bytes ! FAILED\n");
    exit(0);
  } else {
    if (sizeof(char)!=1) {
      printf("char: not 1 byte ! FAILED\n");
      exit(0);
    } else {
      if (sizeof(testarray)!=8) {
	printf("custom array is not 8 bytes !\n");
      } else printf("ok\n");
    }
  }
  printf("OK Checking endian structure ... ");

  if (*((int *)testarray)==0x00000001) {
    printf("OK big endian found.\n");
    exit(1);
  } else {
    if (*((int *)testarray)==0x01000000) {
      printf("OK little endian found.\n");
      exit(2);
    } else {
      printf("check FAILED.\n");
      printf("Unknown CPU architecture.\n");
      exit(0);
    }
  }
}
