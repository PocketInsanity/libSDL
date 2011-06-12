/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_gapivideo.c,v 1.5 2004/02/29 21:54:11 lemure Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <windows.h>

/* Not yet in the mingw32 cross-compile headers */
#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN	4
#endif

#ifndef WS_THICKFRAME
#define WS_THICKFRAME 0
#endif

#include "SDL.h"
#include "SDL_mutex.h"
#include "SDL_syswm.h"
#include "SDL_sysvideo.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_pixels_c.h"
#include "SDL_syswm_c.h"
#include "SDL_sysmouse_c.h"
#include "SDL_dibevents_c.h"
#include "SDL_gapivideo.h"

extern void InitializeDisplayOrientation(void);

/* Initialization/Query functions */
static int GAPI_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **GAPI_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *GAPI_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int GAPI_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void GAPI_VideoQuit(_THIS);


/* Hardware surface functions */
static int GAPI_AllocHWSurface(_THIS, SDL_Surface *surface);
static int GAPI_LockHWSurface(_THIS, SDL_Surface *surface);
static void GAPI_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void GAPI_FreeHWSurface(_THIS, SDL_Surface *surface);

/* Windows message handling functions, will not be processed */
static void GAPI_RealizePalette(_THIS);
static void GAPI_PaletteChanged(_THIS, HWND window);
static void GAPI_WinPAINT(_THIS, HDC hdc);

static void GAPI_UpdateRects(_THIS, int numrects, SDL_Rect *rects); 

static int GAPI_Available(void);
static SDL_VideoDevice *GAPI_CreateDevice(int devindex);

void GAPI_GrabHardwareKeys(BOOL grab);

VideoBootStrap WINGAPI_bootstrap = {
	"wingapi", "WinCE GAPI",
	GAPI_Available, GAPI_CreateDevice
};

static void* _OzoneFrameBuffer = NULL;
static struct GXDisplayProperties _OzoneDisplayProperties;
static char _OzoneAvailable = 0;

typedef struct _RawFrameBufferInfo
{
   WORD wFormat;
   WORD wBPP;
   VOID *pFramePointer;
   int  cxStride;
   int  cyStride;
   int  cxPixels;
   int  cyPixels;
} RawFrameBufferInfo;

/* our logger. log to stdout as well as to the debugger */
void debugLog(const char *txt, ...) {
	char str[256];
	TCHAR txtw[256];
	va_list va;

	va_start(va, txt);
	vsnprintf(str, 256, txt, va);
	va_end(va);

	MultiByteToWideChar(CP_ACP, 0, str, strlen(str) + 1, txtw, 255);

	fprintf(stderr, "%s\n", str);
	OutputDebugString(txtw);
}


struct GXDisplayProperties Ozone_GetDisplayProperties(void) {
	return _OzoneDisplayProperties;
}

int Ozone_OpenDisplay(HWND window, unsigned long flag) {
	return 1;
}

int Ozone_CloseDisplay(void) {
	return 1;
}

void* Ozone_BeginDraw(void) {
	return _OzoneFrameBuffer;
}

int Ozone_EndDraw(void) {
	return 1;
}

int Ozone_Suspend(void) {
	return 1;
}

int Ozone_Resume(void) {
	return 1;
}

static HINSTANCE checkOzone(tGXDisplayProperties *gxGetDisplayProperties, tGXOpenDisplay *gxOpenDisplay,
					  tGXVoidFunction *gxCloseDisplay, tGXBeginDraw *gxBeginDraw, 
					  tGXVoidFunction *gxEndDraw, tGXVoidFunction *gxSuspend, tGXVoidFunction *gxResume) {
#ifdef ARM

	int result;
	RawFrameBufferInfo frameBufferInfo;
	HDC hdc = GetDC(NULL);
	result = ExtEscape(hdc, GETRAWFRAMEBUFFER, 0, NULL, sizeof(RawFrameBufferInfo), (char *)&frameBufferInfo);
	ReleaseDC(NULL, hdc);
	if (result < 0)
		return NULL;
	debugLog("SDL: Running on Ozone");

	_OzoneAvailable = 1;

	// Initializing global parameters
	_OzoneFrameBuffer = frameBufferInfo.pFramePointer;
	_OzoneDisplayProperties.cBPP = frameBufferInfo.wBPP;
	_OzoneDisplayProperties.cbxPitch = frameBufferInfo.cxStride;
	_OzoneDisplayProperties.cbyPitch = frameBufferInfo.cyStride;
	_OzoneDisplayProperties.cxWidth = frameBufferInfo.cxPixels;
	_OzoneDisplayProperties.cyHeight = frameBufferInfo.cyPixels;

	if (frameBufferInfo.wFormat == FORMAT_565)
		_OzoneDisplayProperties.ffFormat = kfDirect565;
	else if (frameBufferInfo.wFormat == FORMAT_555)
		_OzoneDisplayProperties.ffFormat = kfDirect555;
	else {
		debugLog("SDL: Ozone unknown screen format");
		return NULL;
	}

	if (gxGetDisplayProperties)
		*gxGetDisplayProperties = Ozone_GetDisplayProperties;
	if (gxOpenDisplay)
		*gxOpenDisplay = Ozone_OpenDisplay;
	if (gxCloseDisplay)
		*gxCloseDisplay = Ozone_CloseDisplay;
	if (gxBeginDraw)
		*gxBeginDraw = Ozone_BeginDraw;
	if (gxEndDraw)
		*gxEndDraw = Ozone_EndDraw;
	if (gxSuspend)
		*gxSuspend = Ozone_Suspend;
	if (gxResume)
		*gxResume = Ozone_Resume;

	return (HINSTANCE)1;

#else

	return NULL;

#endif
}

int getScreenWidth(_THIS) {
	return displayProperties.cxWidth;
}

int getScreenHeight(_THIS) {
	return displayProperties.cyHeight;
}


/* Check GAPI library */

#define IMPORT(Handle,Variable,Type,Function, Store) \
        Variable = GetProcAddress(Handle, TEXT(Function)); \
		if (!Variable) { \
			FreeLibrary(Handle); \
			return NULL; \
		} \
		if (Store) \
			*Store = (Type)Variable;

static HINSTANCE checkGAPI(tGXDisplayProperties *gxGetDisplayProperties, tGXOpenDisplay *gxOpenDisplay,
					  tGXVoidFunction *gxCloseDisplay, tGXBeginDraw *gxBeginDraw, 
					  tGXVoidFunction *gxEndDraw, tGXVoidFunction *gxSuspend, tGXVoidFunction *gxResume,
					  BOOL bypassOzone) {
	HMODULE gapiLibrary;
	FARPROC proc;
	HINSTANCE result;
	// FIXME paletted !
	tGXDisplayProperties temp_gxGetDisplayProperties;

	// Workaround for Windows Mobile 2003 SE
	_OzoneFrameBuffer = NULL; // FIXME !!
	if (!bypassOzone) {
		result = checkOzone(gxGetDisplayProperties, gxOpenDisplay, gxCloseDisplay, gxBeginDraw, gxEndDraw, gxSuspend, gxResume);
		if (result)
			return result;
	}

	gapiLibrary = LoadLibrary(TEXT("gx.dll"));
	if (!gapiLibrary)
		return NULL;

	IMPORT(gapiLibrary, proc, tGXDisplayProperties, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ", gxGetDisplayProperties)
	IMPORT(gapiLibrary, proc, tGXOpenDisplay, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z", gxOpenDisplay)
	IMPORT(gapiLibrary, proc, tGXVoidFunction, "?GXCloseDisplay@@YAHXZ", gxCloseDisplay)
	IMPORT(gapiLibrary, proc, tGXBeginDraw, "?GXBeginDraw@@YAPAXXZ", gxBeginDraw)
	IMPORT(gapiLibrary, proc, tGXVoidFunction, "?GXEndDraw@@YAHXZ", gxEndDraw)
	IMPORT(gapiLibrary, proc, tGXVoidFunction, "?GXSuspend@@YAHXZ", gxSuspend)
	IMPORT(gapiLibrary, proc, tGXVoidFunction, "?GXResume@@YAHXZ", gxResume)
	
	// FIXME paletted ! for the moment we just bail out	
	if (!gxGetDisplayProperties) {
		IMPORT(gapiLibrary, proc, tGXDisplayProperties, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ", &temp_gxGetDisplayProperties)
		if (temp_gxGetDisplayProperties().ffFormat & kfPalette) {
			FreeLibrary(gapiLibrary);
			return NULL;
		}
		FreeLibrary(gapiLibrary);		
		gapiLibrary = (HINSTANCE)1;
	}
	
	return gapiLibrary;
}


/* GAPI driver bootstrap functions */

static int GAPI_Available(void)
{
	/* Check if the GAPI library is available */
#ifndef GCC_BUILD
	if (!checkGAPI(NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRUE)) {
		debugLog("SDL: GAPI driver not available");
		return 0;
	}
	else {
		debugLog("SDL: GAPI driver available");
		return 1;
	}
#else
	return 1;
#endif
}

static void GAPI_DeleteDevice(SDL_VideoDevice *device)
{
	if (device && device->hidden && device->hidden->gapiFuncs.dynamicGXCloseDisplay)
		device->hidden->gapiFuncs.dynamicGXCloseDisplay();

	if (device && device->hidden)	
		free(device->hidden);
	if (device)
		free(device);

}

static SDL_VideoDevice *GAPI_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;
	debugLog("SDL: Version $Rev$ bootstrapping GAPI device");

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			free(device);
		}
		return(0);
	}
	/* Reset GAPI pointers */
	memset(device->hidden, 0, sizeof(struct SDL_PrivateVideoData));

	/* Set the function pointers */
	device->VideoInit = GAPI_VideoInit;
	device->ListModes = GAPI_ListModes;
	device->SetVideoMode = GAPI_SetVideoMode;
	device->UpdateMouse = WIN_UpdateMouse;
	device->SetColors = GAPI_SetColors;
	device->UpdateRects = NULL;
	device->VideoQuit = GAPI_VideoQuit;
	device->AllocHWSurface = GAPI_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = GAPI_LockHWSurface;
	device->UnlockHWSurface = GAPI_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = GAPI_FreeHWSurface;
	device->SetCaption = WIN_SetWMCaption;
	device->SetIcon = WIN_SetWMIcon;
	device->IconifyWindow = WIN_IconifyWindow;
	device->GrabInput = WIN_GrabInput;
	device->GetWMInfo = WIN_GetWMInfo;
	device->FreeWMCursor = WIN_FreeWMCursor;
	device->CreateWMCursor = WIN_CreateWMCursor;
	device->ShowWMCursor = WIN_ShowWMCursor;
	device->WarpWMCursor = WIN_WarpWMCursor;
	device->CheckMouseMode = WIN_CheckMouseMode;
	device->InitOSKeymap = DIB_InitOSKeymap;
	device->PumpEvents = DIB_PumpEvents;
	device->SetColors = GAPI_SetColors;

	/* Set up the windows message handling functions */
	WIN_RealizePalette = GAPI_RealizePalette;
	WIN_PaletteChanged = GAPI_PaletteChanged;
	WIN_WinPAINT = GAPI_WinPAINT;
	HandleMessage = DIB_HandleMessage;

	device->free = GAPI_DeleteDevice;

	/* set this flag to allow loose checking of modes inside sdl */
	device->handles_any_size = 1;

	return device;
}


int GAPI_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	HINSTANCE ret;
	int gapifound = 0;
	unsigned int mx, my, newmx, newmy;

	/* Create the window */
	if ( DIB_CreateWindow(this) < 0 ) {
		return(-1);
	}

	/* pump some events to get the window drawn ok */
	DIB_PumpEvents(this);

	debugLog("SDL: Starting video access detection --->");
	debugLog("SDL: System %dx%d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

	/* grab the screen dimensions before opening gapi */
	mx = GetSystemMetrics(SM_CXSCREEN);
	my = GetSystemMetrics(SM_CYSCREEN);

	/* first try to get basic gapi support */
	debugLog("SDL: Checking for GAPI");
	ret = checkGAPI(&this->hidden->gapiFuncs.dynamicGXGetDisplayProperties, 
					&this->hidden->gapiFuncs.dynamicGXOpenDisplay, 
					&this->hidden->gapiFuncs.dynamicGXCloseDisplay, 
					&this->hidden->gapiFuncs.dynamicGXBeginDraw, 
					&this->hidden->gapiFuncs.dynamicGXEndDraw, 
					&this->hidden->gapiFuncs.dynamicGXSuspend, 
					&this->hidden->gapiFuncs.dynamicGXResume, 
					TRUE);
	if (ret) {	/* found gapi */
		gapifound = 1;
		this->hidden->gapiFuncs.dynamicGXOpenDisplay(SDL_Window, GX_FULLSCREEN);
		this->hidden->displayProps = this->hidden->gapiFuncs.dynamicGXGetDisplayProperties();
		debugLog("SDL: GAPI OK, %dx%d, H=%d V=%d, %dbpp, landscape %s", displayProperties.cxWidth, displayProperties.cyHeight,
				displayProperties.cbxPitch, displayProperties.cbyPitch, displayProperties.cBPP,
				(displayProperties.ffFormat & kfLandscape) ? "true" : "false");
	}

	/* grab the screen dimensions after opening gapi */
	newmx = GetSystemMetrics(SM_CXSCREEN);
	newmy = GetSystemMetrics(SM_CYSCREEN);

	/* if the reported extents are not directly equal to the system metrics, try ozone */

//	if ( !( ( ((unsigned int) GetSystemMetrics(SM_CXSCREEN) == displayProperties.cxWidth) && ((unsigned int) GetSystemMetrics(SM_CYSCREEN) == displayProperties.cyHeight) ) ||
//	        ( ((unsigned int) GetSystemMetrics(SM_CYSCREEN) == displayProperties.cxWidth) && ((unsigned int) GetSystemMetrics(SM_CXSCREEN) == displayProperties.cyHeight) ) )
//	     || !ret ) {

	if ( newmx != displayProperties.cxWidth || newmy != displayProperties.cyHeight || ! ret) {
		debugLog("SDL: Trying Ozone");
		// gapi reports incorrect extents (or unavailable) so force ozone
		if (!gapifound)
			ret = checkGAPI(&this->hidden->gapiFuncs.dynamicGXGetDisplayProperties, 
					&this->hidden->gapiFuncs.dynamicGXOpenDisplay, 
					&this->hidden->gapiFuncs.dynamicGXCloseDisplay, 
					&this->hidden->gapiFuncs.dynamicGXBeginDraw, 
					&this->hidden->gapiFuncs.dynamicGXEndDraw, 
					&this->hidden->gapiFuncs.dynamicGXSuspend, 
					&this->hidden->gapiFuncs.dynamicGXResume, 
					FALSE);
		else	// keep around some useful stuff from gapi
			ret = checkGAPI(&this->hidden->gapiFuncs.dynamicGXGetDisplayProperties, 
					&this->hidden->gapiFuncs.dynamicGXOpenDisplay, 
					&this->hidden->gapiFuncs.dynamicGXCloseDisplay, 
					&this->hidden->gapiFuncs.dynamicGXBeginDraw, 
					&this->hidden->gapiFuncs.dynamicGXEndDraw, 
					NULL,
					NULL,
					FALSE);
		this->hidden->displayProps = this->hidden->gapiFuncs.dynamicGXGetDisplayProperties();
		debugLog("SDL: Ozone %dx%d", displayProperties.cxWidth, displayProperties.cyHeight);
	}

	/* Ozone is not working ok, fall back to gapi */
	if ( _OzoneAvailable && !( (mx == displayProperties.cxWidth && my == displayProperties.cyHeight) || 
				(mx == displayProperties.cyHeight && my == displayProperties.cxWidth) ) ) {
		_OzoneAvailable = 0;
		debugLog("SDL: Ozone no good, switching back to GAPI");
		ret = checkGAPI(&this->hidden->gapiFuncs.dynamicGXGetDisplayProperties, 
				&this->hidden->gapiFuncs.dynamicGXOpenDisplay, 
				&this->hidden->gapiFuncs.dynamicGXCloseDisplay, 
				&this->hidden->gapiFuncs.dynamicGXBeginDraw, 
				&this->hidden->gapiFuncs.dynamicGXEndDraw, 
				&this->hidden->gapiFuncs.dynamicGXSuspend, 
				&this->hidden->gapiFuncs.dynamicGXResume, 
				TRUE);
		this->hidden->displayProps = this->hidden->gapiFuncs.dynamicGXGetDisplayProperties();
	}

	/* Which will need a tiny input hack if the original code does not have the "Hi Res" aware resource property set */
	ozoneHack = 0;
	if (_OzoneFrameBuffer && (GetSystemMetrics(SM_CXSCREEN) != _OzoneDisplayProperties.cxWidth || GetSystemMetrics(SM_CYSCREEN) != _OzoneDisplayProperties.cyHeight)) {
		debugLog("SDL: Running true Ozone with stylus hack");
		ozoneHack = 1;
	}

	vformat->BitsPerPixel = (unsigned char)displayProperties.cBPP;

	/* if we got to here, the orientation has changed && the framebuffer reports extents using the new orientation, reinit the orientation */
	if (mx != newmx && my != newmy && displayProperties.cxWidth == newmx && displayProperties.cyHeight == newmy)
	{
		debugLog("SDL: Orientation reinit");
		InitializeDisplayOrientation();
	}

	/* print a nice debug info string */
	debugLog("SDL: <----- Detection finished. Running on %s driver at %dx%d (real %d, %d), using %s blitter",
			 _OzoneAvailable ? "Ozone" : "GAPI",
			 displayProperties.cxWidth, displayProperties.cyHeight,
			 GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
#ifdef ARM
			 "ARM accelerated"
#else
			 "fallback fast C"
#endif
	       	);

	// Get color mask
	if (displayProperties.ffFormat & kfDirect565) {
		vformat->BitsPerPixel = 16;
		vformat->Rmask = 0x0000f800;
		vformat->Gmask = 0x000007e0;
		vformat->Bmask = 0x0000001f;
		videoMode = GAPI_DIRECT_565;
	}
	else
	if (displayProperties.ffFormat & kfDirect555) {
		vformat->BitsPerPixel = 16;
		vformat->Rmask = 0x00007c00;
		vformat->Gmask = 0x000003e0;
		vformat->Bmask = 0x0000001f;
		videoMode = GAPI_DIRECT_555;
	}
	else
	if ((displayProperties.ffFormat & kfDirect) && (displayProperties.cBPP <= 8)) {
		// We'll perform the conversion
		vformat->BitsPerPixel = 24;
		vformat->Rmask = 0x00ff0000;
		vformat->Gmask = 0x0000ff00;
		vformat->Bmask = 0x000000ff;
		if (displayProperties.ffFormat & kfDirectInverted)
			invert = (1 << displayProperties.cBPP) - 1;
		colorscale = displayProperties.cBPP < 8 ? 8 - displayProperties.cBPP : 0;
		videoMode = GAPI_MONO;
	}
	else
	if (displayProperties.ffFormat & kfPalette) {
		videoMode = GAPI_PALETTE;
	}

	this->UpdateRects = GAPI_UpdateRects;
	
	/* We're done! */
	return(0);
}

SDL_Rect **GAPI_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	static SDL_Rect res = { -1, -1, -1, -1 }, *resa[] = {&res, (SDL_Rect *)NULL};

	res.w = getScreenWidth(this);
	res.h = getScreenHeight(this);

   	return resa;
}

SDL_Surface *GAPI_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	SDL_Surface *video;
	Uint32 Rmask, Gmask, Bmask;
	Uint32 prev_flags;
	DWORD style;
	const DWORD directstyle = (WS_VISIBLE|WS_POPUP);
	const DWORD windowstyle = (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
	const DWORD resizestyle = (WS_THICKFRAME|WS_MAXIMIZEBOX);
	int screenWidth, screenHeight, x, y;
	Uint8 *scr, *scrold;
#ifdef WINCE_EXTRADEBUG
	debugLog("SDL: Opening mode %dx%d", width, height);
#endif
	/* Recalculate bitmasks if necessary */
	if (bpp == current->format->BitsPerPixel) {
		video = current;
	}
	else {
		switch(bpp) {
			case 8:
				Rmask = 0;
				Gmask = 0;
				Bmask = 0;
				break;
			case 15:				
			case 16:
				/* Default is 565 unless the display is specifically 555 */
				if (displayProperties.ffFormat & kfDirect555) {
					Rmask = 0x00007c00;
					Gmask = 0x000003e0;
					Bmask = 0x0000001f;
				}
				else {
					Rmask = 0x0000f800;
					Gmask = 0x000007e0;
					Bmask = 0x0000001f;
				}
				break;
			case 24:
			case 32:
				Rmask = 0x00ff0000;
				Gmask = 0x0000ff00;
				Bmask = 0x000000ff;
				break;
			default:
				SDL_SetError("Unsupported Bits Per Pixel format requested");
				return NULL;
		}
		video = SDL_CreateRGBSurface(SDL_SWSURFACE,
					0, 0, bpp, Rmask, Gmask, Bmask, 0);
		if ( video == NULL ) {
			SDL_OutOfMemory();
			return(NULL);
		}
	}
	
	/* Fill in part of the video surface */
	prev_flags = video->flags;
	video->flags = SDL_HWPALETTE; /* Clear flags */
	video->w = width;
	video->h = height;
	video->pitch = SDL_CalculatePitch(video);
	mainSurfaceWidth = width;
	mainSurfaceHeight = height;	

	/* Reset the palette and create a new one if necessary */	
	if (screenPal != NULL) {
		DeleteObject(screenPal);
		screenPal = NULL;
	}

	/* See if we need to create a translation palette */
	if (convertPalette != NULL) {
		free(convertPalette);
	}
	if (bpp == 8) {
		debugLog("SDL: creating palette");
		convertPalette = (unsigned short*)malloc(256 * sizeof(unsigned short));
	}

	if (displayProperties.ffFormat & kfPalette) {
		/* Will only be able to support 256 colors in this mode */
		// FIXME
		//screenPal = GAPI_CreatePalette();
	}

	/* Set Window style */
	style = GetWindowLong(SDL_Window, GWL_STYLE);
	if ( (flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ) {
		// do nothing, window has been created for fullscreen
	} else {
		if ( flags & SDL_NOFRAME ) {
			// do nothing, window has been created for fullscreen
			video->flags |= SDL_NOFRAME;
		} else {
			style &= ~directstyle;
			style |= windowstyle;
			if ( flags & SDL_RESIZABLE ) {
				style |= resizestyle;
				video->flags |= SDL_RESIZABLE;
			}
		}
	}

	if (!SDL_windowid && !( (flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ))
	{
		debugLog("SDL: GAPI: Setting new window style");
		SetWindowLong(SDL_Window, GWL_STYLE, style);
	}

	/* Allocate bitmap */
	if (gapiBuffer) {
		free(gapiBuffer);
		gapiBuffer = NULL;
	}
	gapiBuffer = malloc(video->h * video->pitch);
	video->pixels = gapiBuffer;

	/* See if we will rotate */
	if (flags & SDL_LANDSCVIDEO) {
		rotation = SDL_ROTATE_LEFT;
#ifdef WINCE_EXTRADEBUG
		debugLog("SDL: Requested landscape mode");
#endif
	} else if (flags & SDL_INVLNDVIDEO) {
		rotation = SDL_ROTATE_RIGHT;
#ifdef WINCE_EXTRADEBUG
		debugLog("SDL: Requested inverse landscape mode");
#endif
	} else {
		rotation = SDL_ROTATE_NONE;
#ifdef WINCE_EXTRADEBUG
		debugLog("SDL: Requested portrait mode");
#endif
	}
	screenWidth = getScreenWidth(this);
	screenHeight = getScreenHeight(this);
	if (flags & SDL_FULLSCREEN) 
		if (width > screenWidth && width <= screenHeight && rotation == SDL_ROTATE_NONE) {
			rotation = SDL_ROTATE_LEFT;
			if (flags & SDL_INVLNDVIDEO)
				rotation = SDL_ROTATE_RIGHT;
		} else if (width > screenHeight && width <= screenWidth && rotation != SDL_ROTATE_NONE)
			rotation = SDL_ROTATE_NONE;
	/* save into flags */
	switch (rotation) {
		case SDL_ROTATE_NONE:
			video->flags |= SDL_PORTRTVIDEO;
#ifdef WINCE_EXTRADEBUG
			debugLog("SDL: Setting portrait mode");
#endif
			break;
		case SDL_ROTATE_LEFT:
			video->flags |= SDL_LANDSCVIDEO;
#ifdef WINCE_EXTRADEBUG
			debugLog("SDL: Setting landscape mode");
#endif
			break;
		case SDL_ROTATE_RIGHT:
			video->flags |= SDL_INVLNDVIDEO;
#ifdef WINCE_EXTRADEBUG
			debugLog("SDL: Setting inverse landscape mode");
#endif
			break;
	}

	/* Compute padding */
	padWidth = 0;
	padHeight = 0;
	
	if (rotation == SDL_ROTATE_NONE) {
		if (getScreenWidth(this) > width)
			padWidth = (getScreenWidth(this) - width) / 2;
		if (getScreenHeight(this) > height)
			padHeight = (getScreenHeight(this) - height) / 2;
	}
	else {
		if (getScreenWidth(this) > height)
			padHeight = (getScreenWidth(this) - height) / 2;
		if (getScreenHeight(this) > width)
			padWidth = (getScreenHeight(this) - width) / 2;
	}

	/* Compute the different drawing properties */
	switch(rotation) {
		case SDL_ROTATE_NONE:
			dstPixelstep = displayProperties.cbxPitch;
			dstLinestep = displayProperties.cbyPitch;
			startOffset = 0;
			break;
		case SDL_ROTATE_LEFT:
			dstPixelstep = -displayProperties.cbyPitch;
			dstLinestep = displayProperties.cbxPitch;
			startOffset = displayProperties.cbyPitch * (displayProperties.cyHeight - 1);
			break;
		case SDL_ROTATE_RIGHT:
			dstPixelstep = displayProperties.cbyPitch;
			dstLinestep = -displayProperties.cbxPitch;
			startOffset = displayProperties.cbxPitch * displayProperties.cxWidth; // predecrement scheme on inverse landscape. thx ggn :-)
			break;
	}
	startOffset += padWidth * dstPixelstep + padHeight * dstLinestep;
	
	srcLinestep = video->pitch;
	srcPixelstep = (bpp == 15 ? 2 : bpp / 8);
	
	SetWindowPos(SDL_Window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	// especially, do not unhide the window. it is not hidden upon creation

	/* Grab hardware keys if necessary */
	if (flags & SDL_FULLSCREEN)
		GAPI_GrabHardwareKeys(TRUE);

	/* Blank screen */
	scr = GXBeginDraw();
	for (y=0; y<getScreenHeight(this); y++) {
		scrold = scr;
		for (x=0; x<getScreenWidth(this); x++) {
			*(unsigned short int *) scr = 0;
			scr += displayProperties.cbxPitch;
		}
		scr = scrold + displayProperties.cbyPitch;
	}
	//memset(GXBeginDraw(), 0, getScreenWidth(this) * getScreenHeight(this) * 2);
	GXEndDraw();

	/* We're done */
	return(video);
}

/* We don't actually allow hardware surfaces other than the main one */
static int GAPI_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void GAPI_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* We need to wait for vertical retrace on page flipped displays */
static int GAPI_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void GAPI_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* make no assumptions about the layout of the framebuffer (slow but sure) */
static void GAPI_CompatibilityBlit(_THIS, int numrects, SDL_Rect *rects)
{
	static int height = 0, width = 0, wloop = 0;
	static unsigned char *screenBuffer = NULL;
	static unsigned char *destPointer = NULL, *destPointerTmp = NULL;
	static unsigned char *srcPointer = NULL, *srcPointerTmp = NULL;

	screenBuffer = GXBeginDraw();

	while (numrects--) {
		width = rects->w;
		height = rects->h;
		destPointer = screenBuffer + startOffset + (rects->x * dstPixelstep) + (rects->y * dstLinestep);
		srcPointer = (unsigned char*)gapiBuffer + (rects->x * srcPixelstep) + (rects->y * srcLinestep);

		while (height--) {
			destPointerTmp = destPointer;
			srcPointerTmp = srcPointer;
			wloop = width;
			while (wloop--) {
				*(unsigned short *)destPointerTmp = *(unsigned short *)srcPointerTmp;
				destPointerTmp += dstPixelstep;
				srcPointerTmp += srcPixelstep;
			}

			destPointer += dstLinestep;
			srcPointer += srcLinestep;
		}
		rects++;
	}

	GXEndDraw();
}



#ifdef ARM
/* special assembly blitter contributed by Robin Watts */
extern void ARM_rotate(unsigned char *dstptr, unsigned char *srcPtr, int w, int h, int dstLineStep, int srcPixStep, int srcLineStep);
#endif

static void GAPI_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	static int height=0, width=0, w4=0, w4loop=0, w2=0, w1=0, x1=0, h4=0, h4loop=0, h2=0, h1=0, dstPixelstep2=0, dstLinestep2=0, aligned=0, srcPixelstep2=0, dstLinestep2c=0;
	static unsigned char *screenBuffer = NULL;
	static unsigned char *destPointer = NULL, *destPointerTmp = NULL;
	static unsigned char *srcPointer = NULL, *srcPointerTmp = NULL;
	static unsigned int pixels = 0, pixels2 = 0;

	/* paletted blitting not supported */
	if (convertPalette)
		return;

	/* non standard displays get the compatibility (read slow) blitter. sorry :-D */
	if ( (rotation == SDL_ROTATE_NONE && dstPixelstep != 2) ||
	     (rotation == SDL_ROTATE_LEFT && dstLinestep  != 2) ||
	     (rotation == SDL_ROTATE_RIGHT && dstLinestep != -2) )
	{
		static logflag = 0;
		if (logflag == 0) {
			logflag = 1;
			debugLog("SDL: Falling back to compatibility blitter");
		}
		GAPI_CompatibilityBlit(this, numrects, rects);
		return;
	}

	screenBuffer = GXBeginDraw();
	dstPixelstep2 = dstPixelstep << 1;
	srcPixelstep2 = srcPixelstep << 1;
	dstLinestep2 = dstLinestep << 1;
	dstLinestep2c = dstLinestep2 - dstPixelstep;

	while (numrects--) {
#ifndef ARM
		destPointer = screenBuffer + startOffset + (rects->x * dstPixelstep) + (rects->y * dstLinestep);
		srcPointer = (unsigned char*)gapiBuffer + (rects->x * srcPixelstep) + (rects->y * srcLinestep);
		width = rects->w;
		height = rects->h;
		x1 = (((unsigned int)destPointer) & 2) >> 1;
		aligned = !((((unsigned int)srcPointer) & 2) ^ (((unsigned int)destPointer) & 2));	// xnor
#endif
		switch (rotation)
		{
			case SDL_ROTATE_NONE:
#ifdef ARM
				destPointer = screenBuffer + startOffset + (rects->x * dstPixelstep) + (rects->y * dstLinestep);
				srcPointer = (unsigned char*)gapiBuffer + (rects->x * srcPixelstep) + (rects->y * srcLinestep);
				width = rects->w;
				height = rects->h;
#endif
				while (height) {
					memcpy(destPointer, srcPointer, width << 1);
					destPointer += dstLinestep;
					srcPointer += srcLinestep;
					height--;
				}
				break;

			case SDL_ROTATE_LEFT:
#ifdef ARM
				ARM_rotate(screenBuffer + startOffset + ((rects->x+rects->w-1) * dstPixelstep) + (rects->y * dstLinestep),
						(unsigned char*)gapiBuffer + ((rects->x+rects->w-1) * srcPixelstep) + (rects->y * srcLinestep),
						rects->h, rects->w,
						-dstPixelstep,  /* 480 */
						srcLinestep,    /* 640 */
						-srcPixelstep); /*  -2 */
#else
				h4 = (height - x1) >> 2;
				h2 = (height - x1) & 2;
				h1 = (height - x1) & 1;
				while (width > 0) {
					srcPointerTmp = srcPointer;
					destPointerTmp = destPointer;
					if (!aligned || width == 1 || (((unsigned int) srcPointer) & 2)) {
						if (x1) {
							*(unsigned short*)destPointer = *(unsigned short *)srcPointer; destPointer += dstLinestep; srcPointer += srcLinestep; }
						for (h4loop = h4; h4loop > 0; h4loop--) {
							pixels = *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							pixels |= ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = pixels; destPointer += dstLinestep2;
							pixels = *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							pixels |= ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = pixels; destPointer += dstLinestep2;
						}
						if (h2) {
							pixels = *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							pixels |= ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = pixels; destPointer += dstLinestep2;
						}
						if (h1) 
							*(unsigned short*)destPointer = *(unsigned short *)srcPointer; 
						srcPointer = srcPointerTmp + srcPixelstep;
						destPointer = destPointerTmp + dstPixelstep;
						width--;
					} else {
						if (x1) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							*(unsigned short*)destPointer = (unsigned short) pixels; destPointer += dstPixelstep;
							*(unsigned short*)destPointer = (unsigned short) (pixels >> 16); destPointer += dstLinestep - dstPixelstep;
						}
						for (h4loop = h4; h4loop > 0; h4loop--) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = (pixels2 << 16) | ((short unsigned int) pixels); destPointer += dstPixelstep;
							*(unsigned int*)destPointer = (pixels2 & 0xFFFF0000) | (pixels >> 16); destPointer += dstLinestep2c;
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = (pixels2 << 16) | ((short unsigned int) pixels); destPointer += dstPixelstep;
							*(unsigned int*)destPointer = (pixels2 & 0xFFFF0000) | (pixels >> 16); destPointer += dstLinestep2c;
						}
						if (h2) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							*(unsigned int*)destPointer = (pixels2 << 16) | ((short unsigned int) pixels); destPointer += dstPixelstep;
							*(unsigned int*)destPointer = (pixels2 & 0xFFFF0000) | (pixels >> 16); destPointer += dstLinestep2c;
						}
						if (h1) {
							pixels = *(unsigned int*)srcPointer;
							*(unsigned short*)destPointer = (unsigned short) pixels; destPointer += dstPixelstep;
							*(unsigned short*)destPointer = (unsigned short) (pixels >> 16); 
						}
						srcPointer = srcPointerTmp + srcPixelstep2;
						destPointer = destPointerTmp + dstPixelstep2;
						width -= 2;
					}
				}
#endif
				break;

			case SDL_ROTATE_RIGHT:
#ifdef ARM
				ARM_rotate(screenBuffer + startOffset - 2 + (rects->x * dstPixelstep) + ((rects->y+rects->h-1) * dstLinestep),
						(unsigned char*)gapiBuffer     + (rects->x * srcPixelstep) + ((rects->y+rects->h-1) * srcLinestep),
						rects->h, rects->w,
						dstPixelstep,  /*  480 */
						-srcLinestep,  /* -480 */
						srcPixelstep); /*    2 */
#else
				h4 = (height - x1) >> 2;
				h2 = (height - x1) & 2;
				h1 = (height - x1) & 1;
				while (width > 0) {
					srcPointerTmp = srcPointer;
					destPointerTmp = destPointer;
					if (!aligned || width == 1 || (((unsigned int) srcPointer) & 2)) {
						if (x1) {
							destPointer += dstLinestep; *(unsigned short*)destPointer = *(unsigned short *)srcPointer; srcPointer += srcLinestep; }
						for (h4loop = h4; h4loop > 0; h4loop--) {
							pixels = ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							pixels |= *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2; *(unsigned int*)destPointer = pixels; 
							pixels = ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							pixels |= *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2; *(unsigned int*)destPointer = pixels;
						}
						if (h2) {
							pixels = ((unsigned int)*(unsigned short*)srcPointer)<<16; srcPointer += srcLinestep;
							pixels |= *(unsigned short*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2; *(unsigned int*)destPointer = pixels;
						}
						if (h1) { 
							destPointer += dstLinestep; *(unsigned short*)destPointer = *(unsigned short *)srcPointer; }
						srcPointer = srcPointerTmp + srcPixelstep;
						destPointer = destPointerTmp + dstPixelstep;
						width--;
					} else {
						if (x1) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep; *(unsigned short*)destPointer = (unsigned short) pixels;
							destPointer += dstPixelstep; *(unsigned short*)destPointer = (unsigned short) (pixels >> 16);
						} else if (h4 || h2)
							destPointer += dstPixelstep;
						for (h4loop = h4; h4loop > 0; h4loop--) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2c;*(unsigned int*)destPointer = (pixels << 16) | ((short unsigned int) pixels2);
							destPointer += dstPixelstep; *(unsigned int*)destPointer = (pixels & 0xFFFF0000) | (pixels2 >> 16);
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2c;*(unsigned int*)destPointer = (pixels << 16) | ((short unsigned int) pixels2);
							destPointer += dstPixelstep; *(unsigned int*)destPointer = (pixels & 0xFFFF0000) | (pixels2 >> 16);
						}
						if (h2) {
							pixels = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							pixels2 = *(unsigned int*)srcPointer; srcPointer += srcLinestep;
							destPointer += dstLinestep2c;*(unsigned int*)destPointer = (pixels << 16) | ((short unsigned int) pixels2);
							destPointer += dstPixelstep; *(unsigned int*)destPointer = (pixels & 0xFFFF0000) | (pixels2 >> 16);
						}
						if (h4 || h2) destPointer -= dstPixelstep;
						if (h1) { 
							pixels = *(unsigned int*)srcPointer;
							destPointer += dstLinestep; *(unsigned short*)destPointer = (unsigned short) pixels;
							destPointer += dstPixelstep; *(unsigned short*)destPointer = (unsigned short) (pixels >> 16);
						}
						srcPointer = srcPointerTmp + srcPixelstep2;
						destPointer = destPointerTmp + dstPixelstep2;
						width -= 2;
					}
				}
#endif
		}
		rects++;
	}

	GXEndDraw();
}

/* -------------------------------------------------------------------------------- */
// Global fixme for paletted mode !

#define COLORCONV565(r,g,b) (((r&0xf8)<<(11-3))|((g&0xfc)<<(5-2))|((b&0xf8)>>3))

#define COLORCONV555(r,g,b) (((r&0xf8)<<(10-3))|((g&0xf8)<<(5-2))|((b&0xf8)>>3))

int GAPI_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	int i;
	/* Convert colors to appropriate 565 or 555 mapping */
	for (i=0; i<ncolors; i++) 
		convertPalette[firstcolor + i] = (videoMode == GAPI_DIRECT_565 ? 
			COLORCONV565(colors[i].r, colors[i].g, colors[i].b) :
			COLORCONV555(colors[i].r, colors[i].g, colors[i].b));
	return(1);
}

static void GAPI_RealizePalette(_THIS)
{
	debugLog("SDL: GAPI_RealizePalette NOT IMPLEMENTED");
}

static void GAPI_PaletteChanged(_THIS, HWND window)
{
	debugLog("SDL: GAPI_PaletteChanged NOT IMPLEMENTED");
}

/* Exported for the windows message loop only */
static void GAPI_WinPAINT(_THIS, HDC hdc)
{
	debugLog("SDL: GAPI_WinPAINT NOT IMPLEMENTED");
}


/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void GAPI_VideoQuit(_THIS)
{
	/* Destroy the window and everything associated with it */
	if ( SDL_Window ) {
		/* Delete the screen bitmap (also frees screen->pixels) */
		if ( this->screen ) {
			if ( this->screen->flags & SDL_FULLSCREEN )
				GAPI_GrabHardwareKeys(FALSE);

			if (this->screen->pixels != NULL)
			{
				free(this->screen->pixels);
				this->screen->pixels = NULL;
			}

			if (GXCloseDisplay)
				GXCloseDisplay();
		}
	}
}

void GAPI_GrabHardwareKeys(BOOL grab) {
	HINSTANCE GAPI_handle;
	tGXVoidFunction GAPIActionInput;

	GAPI_handle = LoadLibrary(TEXT("gx.dll"));
	if (!GAPI_handle)
		return;
	GAPIActionInput = (tGXVoidFunction)GetProcAddress(GAPI_handle, (grab ? TEXT("?GXOpenInput@@YAHXZ") : TEXT("?GXCloseInput@@YAHXZ")));
	if (GAPIActionInput) {
		GAPIActionInput();
	}
	FreeLibrary(GAPI_handle);
}
