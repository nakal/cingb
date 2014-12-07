
/*
  Won't work and even compile
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <ddrawex.h>

#include "globals.h"

int initsys(void) {

  /*	HRESULT 	hr;
	IDirectDraw 	*pDD;    
	IDirectDraw3 	*pDD3; 
	IDirectDrawFactory *pDDF;

	CoInitialize(NULL);


	CoCreateInstance(CLSID_DirectDrawFactory, NULL, CLSCTX_INPROC_SERVER, 
							IID_IDirectDrawFactory, (void **)&pDDF);

	hr = (pDDF->CreateDirectDraw(NULL, GetDesktopWindow(), DDSCL_NORMAL, 
				NULL, NULL, &pDD));

	if (hr !=DD_OK) {
	}
	
	hr =(pDD->QueryInterface(IID_IDirectDraw3, (LPVOID*)&pDD3));
	
	if (hr !=S_OK) {
	}
	
	pDD->Release();
	pDD= NULL;	

	ZeroMemory(&ddsd, sizeof(ddsd));
      ddsd.dwSize = sizeof(ddsd);    
	ddsd.dwFlags = DDSD_CAPS;
      ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE; 
      hr = pDD3->CreateSurface(&ddsd, &pPrimarySurface, NULL);
	

	pDD3->Release();
	pDDF->Release();
	pPrimarySurface->Release();    

	CoUninitialize();
  */
	return 0;
}

unsigned long int GB_STDPAL[4];
int resolution=0;
int doublesize=0;

void vramdump(tilescreen,dataofs)
int tilescreen;
int dataofs;
{
  tilescreen=0;
  dataofs=0;
  return;
}

void drawscreen(void) {
}

void joypad(void) {
}

void donesys(void) {
}

ulong color_translate(uint gbcol) {
	return 0L;
}

