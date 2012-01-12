/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if defined(UNDER_CE) && (_WIN32_WCE >= 300)
/*#include <aygshell.h>                      // Add Pocket PC includes
#pragma comment( lib, "aygshell" )         // Link Pocket PC library
*/
#include <windows.h>
#endif
#ifdef _MSC_VER
#define inline __inline
#endif

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
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "SDL_dibvideo.h"
#include "../wincommon/SDL_syswm_c.h"
#include "../wincommon/SDL_sysmouse_c.h"
#include "SDL_dibevents_c.h"
#include "../wincommon/SDL_wingl_c.h"

#ifdef _WIN32_WCE
#define NO_GETDIBITS
#define NO_GAMMA_SUPPORT
  #if _WIN32_WCE < 420
    #define NO_CHANGEDISPLAYSETTINGS
  #else
    #define ChangeDisplaySettings(lpDevMode, dwFlags) ChangeDisplaySettingsEx(NULL, (lpDevMode), 0, (dwFlags), 0)
  #endif

/* uncomment this line if you target WinCE 3.x platform: */
//#define NO_SETDIBCOLORTABLE

/* these 2 variables are used to suport paletted DIBs on WinCE 3.x that 
   does not implement SetDIBColorTable, and when SetDIBColorTable is not working.
   Slow. DIB is recreated every time.
*/
static BITMAPINFO *last_bitmapinfo;
static void** last_bits;

#endif
#ifndef WS_MAXIMIZE
#define WS_MAXIMIZE	0
#endif
#ifndef WS_THICKFRAME
#define WS_THICKFRAME	0
#endif
#ifndef SWP_NOCOPYBITS
#define SWP_NOCOPYBITS	0
#endif
#ifndef PC_NOCOLLAPSE
#define PC_NOCOLLAPSE	0
#endif

#ifdef _WIN32_WCE
// defined and used in SDL_sysevents.c
extern HINSTANCE aygshell;
#endif

/* Initialization/Query functions */
static int DIB_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **DIB_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
SDL_Surface *DIB_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int DIB_SetColors(_THIS, int firstcolor, int ncolors,
			 SDL_Color *colors);
static void DIB_CheckGamma(_THIS);
void DIB_SwapGamma(_THIS);
void DIB_QuitGamma(_THIS);
int DIB_SetGammaRamp(_THIS, Uint16 *ramp);
int DIB_GetGammaRamp(_THIS, Uint16 *ramp);
static void DIB_VideoQuit(_THIS);

/* Hardware surface functions */
static int DIB_AllocHWSurface(_THIS, SDL_Surface *surface);
static int DIB_LockHWSurface(_THIS, SDL_Surface *surface);
static void DIB_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void DIB_FreeHWSurface(_THIS, SDL_Surface *surface);

/* Windows message handling functions */
static void DIB_RealizePalette(_THIS);
static void DIB_PaletteChanged(_THIS, HWND window);
static void DIB_WinPAINT(_THIS, HDC hdc);

/* helper fn */
static int DIB_SussScreenDepth();

#ifdef _WIN32_WCE
void DIB_ShowTaskBar(BOOL taskBarShown);
#ifdef ENABLE_WINGAPI
extern void GAPI_GrabHardwareKeys(BOOL grab);
#endif
#endif

/* DIB driver bootstrap functions */

static int DIB_Available(void)
{
	return(1);
}

static void DIB_DeleteDevice(SDL_VideoDevice *device)
{
	if ( device ) {
		if ( device->hidden ) {
			SDL_free(device->hidden);
		}
		if ( device->gl_data ) {
			SDL_free(device->gl_data);
		}
		SDL_free(device);
	}
}

static SDL_VideoDevice *DIB_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
		device->gl_data = (struct SDL_PrivateGLData *)
				SDL_malloc((sizeof *device->gl_data));
	}
	if ( (device == NULL) || (device->hidden == NULL) ||
		                 (device->gl_data == NULL) ) {
		SDL_OutOfMemory();
		DIB_DeleteDevice(device);
		return(NULL);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));
	SDL_memset(device->gl_data, 0, (sizeof *device->gl_data));

	/* Set the function pointers */
	device->VideoInit = DIB_VideoInit;
	device->ListModes = DIB_ListModes;
	device->SetVideoMode = DIB_SetVideoMode;
	device->UpdateMouse = WIN_UpdateMouse;
	device->SetColors = DIB_SetColors;
	device->UpdateRects = NULL;
	device->VideoQuit = DIB_VideoQuit;
	device->AllocHWSurface = DIB_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = DIB_LockHWSurface;
	device->UnlockHWSurface = DIB_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = DIB_FreeHWSurface;
	device->SetGammaRamp = DIB_SetGammaRamp;
	device->GetGammaRamp = DIB_GetGammaRamp;
#if SDL_VIDEO_OPENGL
	device->GL_LoadLibrary = WIN_GL_LoadLibrary;
	device->GL_GetProcAddress = WIN_GL_GetProcAddress;
	device->GL_GetAttribute = WIN_GL_GetAttribute;
	device->GL_MakeCurrent = WIN_GL_MakeCurrent;
	device->GL_SwapBuffers = WIN_GL_SwapBuffers;
#endif
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

	/* Set up the windows message handling functions */
	WIN_RealizePalette = DIB_RealizePalette;
	WIN_PaletteChanged = DIB_PaletteChanged;
	WIN_WinPAINT = DIB_WinPAINT;
	HandleMessage = DIB_HandleMessage;

	device->free = DIB_DeleteDevice;

	/* We're finally ready */
	return device;
}

VideoBootStrap WINDIB_bootstrap = {
	"windib", "Win95/98/NT/2000/CE GDI",
	DIB_Available, DIB_CreateDevice
};

static int cmpmodes(const void *va, const void *vb)
{
    SDL_Rect *a = *(SDL_Rect **)va;
    SDL_Rect *b = *(SDL_Rect **)vb;
    if ( a->w == b->w )
        return b->h - a->h;
    else
        return b->w - a->w;
}

static int DIB_AddMode(_THIS, int bpp, int w, int h)
{
	SDL_Rect *mode;
	int i, index;
	int next_mode;

	/* Check to see if we already have this mode */
	if ( bpp < 8 ) {  /* Not supported */
		return(0);
	}
	index = ((bpp+7)/8)-1;
	for ( i=0; i<SDL_nummodes[index]; ++i ) {
		mode = SDL_modelist[index][i];
		if ( (mode->w == w) && (mode->h == h) ) {
			return(0);
		}
	}

	/* Set up the new video mode rectangle */
	mode = (SDL_Rect *)SDL_malloc(sizeof *mode);
	if ( mode == NULL ) {
		SDL_OutOfMemory();
		return(-1);
	}
	mode->x = 0;
	mode->y = 0;
	mode->w = w;
	mode->h = h;

	/* Allocate the new list of modes, and fill in the new mode */
	next_mode = SDL_nummodes[index];
	SDL_modelist[index] = (SDL_Rect **)
	       SDL_realloc(SDL_modelist[index], (1+next_mode+1)*sizeof(SDL_Rect *));
	if ( SDL_modelist[index] == NULL ) {
		SDL_OutOfMemory();
		SDL_nummodes[index] = 0;
		SDL_free(mode);
		return(-1);
	}
	SDL_modelist[index][next_mode] = mode;
	SDL_modelist[index][next_mode+1] = NULL;
	SDL_nummodes[index]++;

	return(0);
}

static HPALETTE DIB_CreatePalette(int bpp)
{
/*	RJR: March 28, 2000
	moved palette creation here from "DIB_VideoInit" */

	HPALETTE handle = NULL;
	
	if ( bpp <= 8 )
	{
		LOGPALETTE *palette;
		HDC hdc;
		int ncolors;
		int i;

		ncolors = 1;
		for ( i=0; i<bpp; ++i ) {
			ncolors *= 2;
		}
		palette = (LOGPALETTE *)SDL_malloc(sizeof(*palette)+
					ncolors*sizeof(PALETTEENTRY));
		palette->palVersion = 0x300;
		palette->palNumEntries = ncolors;
		hdc = GetDC(SDL_Window);
		GetSystemPaletteEntries(hdc, 0, ncolors, palette->palPalEntry);
		ReleaseDC(SDL_Window, hdc);
		handle = CreatePalette(palette);
		SDL_free(palette);
	}
	
	return handle;
}

int DIB_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
#ifndef NO_CHANGEDISPLAYSETTINGS
	int i;
	DEVMODE settings;
#endif

	/* Create the window */
	if ( DIB_CreateWindow(this) < 0 ) {
		return(-1);
	}

#if !SDL_AUDIO_DISABLED
	DX5_SoundFocus(SDL_Window);
#endif

	/* Determine the screen depth */
	vformat->BitsPerPixel = DIB_SussScreenDepth();
	switch (vformat->BitsPerPixel) {
		case 15:
			vformat->Rmask = 0x00007c00;
			vformat->Gmask = 0x000003e0;
			vformat->Bmask = 0x0000001f;
			vformat->BitsPerPixel = 16;
			break;
		case 16:
			vformat->Rmask = 0x0000f800;
			vformat->Gmask = 0x000007e0;
			vformat->Bmask = 0x0000001f;
			break;
		case 24:
		case 32:
			/* GDI defined as 8-8-8 */
			vformat->Rmask = 0x00ff0000;
			vformat->Gmask = 0x0000ff00;
			vformat->Bmask = 0x000000ff;
			break;
		default:
			break;
	}

	/* See if gamma is supported on this screen */
	DIB_CheckGamma(this);

#ifndef NO_CHANGEDISPLAYSETTINGS

	settings.dmSize = sizeof(DEVMODE);
	settings.dmDriverExtra = 0;
#ifdef _WIN32_WCE
	settings.dmFields = DM_DISPLAYQUERYORIENTATION;
	this->hidden->supportRotation = ChangeDisplaySettingsEx(NULL, &settings, NULL, CDS_TEST, NULL) == DISP_CHANGE_SUCCESSFUL;
#endif
	/* Query for the desktop resolution */
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &SDL_desktop_mode);
	this->info.current_w = SDL_desktop_mode.dmPelsWidth;
	this->info.current_h = SDL_desktop_mode.dmPelsHeight;

	/* Query for the list of available video modes */
	for ( i=0; EnumDisplaySettings(NULL, i, &settings); ++i ) {
		DIB_AddMode(this, settings.dmBitsPerPel,
			settings.dmPelsWidth, settings.dmPelsHeight);
#ifdef _WIN32_WCE		
		if( this->hidden->supportRotation )
			DIB_AddMode(this, settings.dmBitsPerPel,
				settings.dmPelsHeight, settings.dmPelsWidth);
#endif
	}
	/* Sort the mode lists */
	for ( i=0; i<NUM_MODELISTS; ++i ) {
		if ( SDL_nummodes[i] > 0 ) {
			SDL_qsort(SDL_modelist[i], SDL_nummodes[i], sizeof *SDL_modelist[i], cmpmodes);
		}
	}
#else
	// WinCE and fullscreen mode:
	// We use only vformat->BitsPerPixel that allow SDL to
	// emulate other bpp (8, 32) and use triple buffer, 
	// because SDL surface conversion is much faster than the WinCE one.
	// Although it should be tested on devices with graphics accelerator.

	DIB_AddMode(this, vformat->BitsPerPixel,
			GetDeviceCaps(GetDC(NULL), HORZRES), 
			GetDeviceCaps(GetDC(NULL), VERTRES));

#endif /* !NO_CHANGEDISPLAYSETTINGS */

	/* Grab an identity palette if we are in a palettized mode */
	if ( vformat->BitsPerPixel <= 8 ) {
	/*	RJR: March 28, 2000
		moved palette creation to "DIB_CreatePalette" */
		screen_pal = DIB_CreatePalette(vformat->BitsPerPixel);
	}

	/* Fill in some window manager capabilities */
	this->info.wm_available = 1;

#ifdef _WIN32_WCE
	this->hidden->origRotation = -1;
#endif

	/* We're done! */
	return(0);
}

/* We support any format at any dimension */
SDL_Rect **DIB_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	if ( (flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ) {
		return(SDL_modelist[((format->BitsPerPixel+7)/8)-1]);
	} else {
		return((SDL_Rect **)-1);
	}
}

#ifdef _WIN32_WCE

void DIB_ShowTaskBar(BOOL taskBarShown) {
#if !defined(WIN32_PLATFORM_PSPC) || (_WIN32_WCE < 300)
	// Hide taskbar, WinCE 2.x style - from EasyCE
	HKEY hKey=0;
	DWORD dwValue = 0;
	unsigned long lSize = sizeof( DWORD );
	DWORD dwType = REG_DWORD;
	HWND hWnd;

	RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("\\software\\microsoft\\shell"), 0, KEY_ALL_ACCESS, &hKey );
	RegQueryValueEx( hKey, TEXT("TBOpt"), 0, &dwType, (BYTE*)&dwValue, &lSize );
	if (taskBarShown)
		dwValue &= 0xFFFFFFFF - 8;	// reset bit to show taskbar
    else 
		dwValue |= 8;	// set bit to hide taskbar
	RegSetValueEx( hKey, TEXT("TBOpt"), 0, REG_DWORD, (BYTE*)&dwValue, lSize );
	hWnd = FindWindow( TEXT("HHTaskBar"), NULL );
	SendMessage(hWnd, WM_COMMAND, 0x03EA, 0 );
	SetForegroundWindow(SDL_Window);
#else
	if (taskBarShown) 
		SHFullScreen(SDL_Window, SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON);
	else 
		SHFullScreen(SDL_Window, SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON | SHFS_HIDESTARTICON);
#endif
	if (FindWindow(TEXT("HHTaskBar"), NULL)) { // is it valid for HPC ?
		if (taskBarShown) 
			ShowWindow(FindWindow(TEXT("HHTaskBar"),NULL),SW_SHOWNORMAL);
		else 
			ShowWindow(FindWindow(TEXT("HHTaskBar"),NULL),SW_HIDE);
	}
}

#endif

/*
  Helper fn to work out which screen depth windows is currently using.
  15 bit mode is considered 555 format, 16 bit is 565.
  returns 0 for unknown mode.
  (Derived from code in sept 1999 Windows Developer Journal
  http://www.wdj.com/code/archive.html)
*/
static int DIB_SussScreenDepth()
{
#ifdef NO_GETDIBITS
	int depth;
	HDC hdc;

	hdc = GetDC(SDL_Window);
	depth = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(SDL_Window, hdc);
	return(depth);
#else
    int dib_size;
    LPBITMAPINFOHEADER dib_hdr;
    HDC hdc;
    HBITMAP hbm;

    /* Allocate enough space for a DIB header plus palette (for
     * 8-bit modes) or bitfields (for 16- and 32-bit modes)
     */
    dib_size = sizeof(BITMAPINFOHEADER) + 256 * sizeof (RGBQUAD);
    dib_hdr = (LPBITMAPINFOHEADER) SDL_malloc(dib_size);
    SDL_memset(dib_hdr, 0, dib_size);
    dib_hdr->biSize = sizeof(BITMAPINFOHEADER);
    
    /* Get a device-dependent bitmap that's compatible with the
       screen.
     */
    hdc = GetDC(NULL);
    hbm = CreateCompatibleBitmap( hdc, 1, 1 );

    /* Convert the DDB to a DIB.  We need to call GetDIBits twice:
     * the first call just fills in the BITMAPINFOHEADER; the 
     * second fills in the bitfields or palette.
     */
    GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO) dib_hdr, DIB_RGB_COLORS);
    GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO) dib_hdr, DIB_RGB_COLORS);
    DeleteObject(hbm);
    ReleaseDC(NULL, hdc);

    switch( dib_hdr->biBitCount )
    {
    case 8:     return 8;
    case 24:    return 24;
    case 32:    return 32;
    case 16:
        if( dib_hdr->biCompression == BI_BITFIELDS ) {
            /* check the red mask */
            switch( ((DWORD*)((char*)dib_hdr + dib_hdr->biSize))[0] ) {
                case 0xf800: return 16;    /* 565 */
                case 0x7c00: return 15;    /* 555 */
            }
        }
    }
    return 0;    /* poo. */
#endif /* NO_GETDIBITS */
}


/* Various screen update functions available */
static void DIB_NormalUpdate(_THIS, int numrects, SDL_Rect *rects);
static void DIB_RotatedUpdate(_THIS, int numrects, SDL_Rect *rects);

SDL_Surface *DIB_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	SDL_Surface *video;
	Uint32 prev_flags;
	DWORD style;
	const DWORD directstyle =
			(WS_POPUP);
	const DWORD windowstyle = 
			(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
	const DWORD resizestyle =
			(WS_THICKFRAME|WS_MAXIMIZEBOX);
	int binfo_size;
	BITMAPINFO *binfo;
	HDC hdc;
	RECT bounds;
	int x, y;
	Uint32 Rmask, Gmask, Bmask;
	int screenWidth, screenHeight, i;

#ifdef HAVE_OPENGL
	/* Clean up any GL context that may be hanging around */
	if ( current->flags & SDL_OPENGL ) {
		WIN_GL_ShutDown(this);
	}
	SDL_resizing = 1;
#endif

	/* Recalculate the bitmasks if necessary */
	if ( bpp == current->format->BitsPerPixel ) {
		video = current;
	} else {
		switch (bpp) {
			case 15:
			case 16:
				if ( DIB_SussScreenDepth() == 15 ) {
					/* 5-5-5 */
					Rmask = 0x00007c00;
					Gmask = 0x000003e0;
					Bmask = 0x0000001f;
				} else {
					/* 5-6-5 */
					Rmask = 0x0000f800;
					Gmask = 0x000007e0;
					Bmask = 0x0000001f;
				}
				break;
			case 24:
			case 32:
				/* GDI defined as 8-8-8 */
				Rmask = 0x00ff0000;
				Gmask = 0x0000ff00;
				Bmask = 0x000000ff;
				break;
			default:
				Rmask = 0x00000000;
				Gmask = 0x00000000;
				Bmask = 0x00000000;
				break;
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
	video->flags = 0;	/* Clear flags */
	video->w = width;
	video->h = height;
	video->pitch = SDL_CalculatePitch(video);

	/* Small fix for WinCE/Win32 - when activating window
	   SDL_VideoSurface is equal to zero, so activating code
	   is not called properly for fullscreen windows because
	   macros WINDIB_FULLSCREEN uses SDL_VideoSurface
	*/
	SDL_VideoSurface = video;

#if defined(_WIN32_WCE)
	 /* Stuff to hide that $#!^%#$ WinCE taskbar in fullscreen... */
	if ( flags & SDL_FULLSCREEN ) {
		if ( !(prev_flags & SDL_FULLSCREEN) ) {
			DIB_ShowTaskBar(FALSE);
		}
		video->flags |= SDL_FULLSCREEN;
	} else {
		if ( prev_flags & SDL_FULLSCREEN ) {
			DIB_ShowTaskBar(TRUE);
		}
	}
#endif
#ifndef NO_CHANGEDISPLAYSETTINGS
	/* Set fullscreen mode if appropriate */
	if ( (flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ) {
		DEVMODE settings;
		BOOL changed;

		SDL_memset(&settings, 0, sizeof(DEVMODE));
		settings.dmSize = sizeof(DEVMODE);

#ifdef _WIN32_WCE
		// try to rotate screen to fit requested resolution
		if( this->hidden->supportRotation )
		{
			DWORD rotation;

			// ask current mode
			settings.dmFields = DM_DISPLAYORIENTATION;
			ChangeDisplaySettingsEx(NULL, &settings, NULL, CDS_TEST, NULL);
			rotation = settings.dmDisplayOrientation;

			if( (width > GetDeviceCaps(GetDC(NULL), HORZRES))
				&& (height < GetDeviceCaps(GetDC(NULL), VERTRES)))
			{
				switch( rotation )
				{
				case DMDO_0:
					settings.dmDisplayOrientation = DMDO_90;
					break;
				case DMDO_270:
					settings.dmDisplayOrientation = DMDO_180;
					break;
				}
				if( settings.dmDisplayOrientation != rotation )
				{
					// go to landscape
					this->hidden->origRotation = rotation;
					ChangeDisplaySettingsEx(NULL,&settings,NULL,CDS_RESET,NULL);
				}
			}
			if( (width < GetDeviceCaps(GetDC(NULL), HORZRES))
				&& (height > GetDeviceCaps(GetDC(NULL), VERTRES)))
			{
				switch( rotation )
				{
				case DMDO_90:
					settings.dmDisplayOrientation = DMDO_0;
					break;
				case DMDO_180:
					settings.dmDisplayOrientation = DMDO_270;
					break;
				}
				if( settings.dmDisplayOrientation != rotation )
				{
					// go to portrait
					this->hidden->origRotation = rotation;
					ChangeDisplaySettingsEx(NULL,&settings,NULL,CDS_RESET,NULL);
				}
			}

		}
#endif

#ifndef _WIN32_WCE
		settings.dmBitsPerPel = video->format->BitsPerPixel;
		settings.dmPelsWidth = width;
		settings.dmPelsHeight = height;
		settings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
		if ( width <= (int)SDL_desktop_mode.dmPelsWidth &&
		     height <= (int)SDL_desktop_mode.dmPelsHeight ) {
			settings.dmDisplayFrequency = SDL_desktop_mode.dmDisplayFrequency;
			settings.dmFields |= DM_DISPLAYFREQUENCY;
		}
		changed = (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL);
		if ( ! changed && (settings.dmFields & DM_DISPLAYFREQUENCY) ) {
			settings.dmFields &= ~DM_DISPLAYFREQUENCY;
			changed = (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL);
		}
#else
		changed = 1;
#endif
		if ( changed ) {
			video->flags |= SDL_FULLSCREEN;
			SDL_fullscreen_mode = settings;
		}

	}
#endif /* !NO_CHANGEDISPLAYSETTINGS */

	/* Reset the palette and create a new one if necessary */
	if ( screen_pal != NULL ) {
	/*	RJR: March 28, 2000
		delete identity palette if switching from a palettized mode */
		DeleteObject(screen_pal);
		screen_pal = NULL;
	}

	if ( bpp <= 8 )
	{
	/*	RJR: March 28, 2000
		create identity palette switching to a palettized mode */
		screen_pal = DIB_CreatePalette(bpp);
	}

	style = GetWindowLong(SDL_Window, GWL_STYLE);
	style &= ~(resizestyle|WS_MAXIMIZE);
	if ( (video->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ) {
		style &= ~windowstyle;
		style |= directstyle;
	} else {
#ifndef NO_CHANGEDISPLAYSETTINGS
		if ( (prev_flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ) {
			ChangeDisplaySettings(NULL, 0);
		}
#endif
		if ( flags & SDL_NOFRAME ) {
			style &= ~windowstyle;
			style |= directstyle;
			video->flags |= SDL_NOFRAME;
		} else {
			style &= ~directstyle;
			style |= windowstyle;
			if ( flags & SDL_RESIZABLE ) {
				style |= resizestyle;
				video->flags |= SDL_RESIZABLE;
			}
		}
#if WS_MAXIMIZE
//		if (IsZoomed(SDL_Window)) style |= WS_MAXIMIZE;
#endif
	}

	/* DJM: Don't piss of anyone who has setup his own window */
	if ( !SDL_windowid )
		SetWindowLong(SDL_Window, GWL_STYLE, style);

	/* Delete the old bitmap if necessary */
	if ( screen_bmp != NULL ) {
		DeleteObject(screen_bmp);
	}
	if ( ! (flags & SDL_OPENGL) ) {
		BOOL is16bitmode = (video->format->BytesPerPixel == 2);

		/* Suss out the bitmap info header */
		binfo_size = sizeof(*binfo);
		if( is16bitmode ) {
			/* 16bit modes, palette area used for rgb bitmasks */
			binfo_size += 3*sizeof(DWORD);
		} else if ( video->format->palette ) {
			binfo_size += video->format->palette->ncolors *
							sizeof(RGBQUAD);
		}
		binfo = (BITMAPINFO *)SDL_malloc(binfo_size);
		if ( ! binfo ) {
			if ( video != current ) {
				SDL_FreeSurface(video);
			}
			SDL_OutOfMemory();
			return(NULL);
		}

		binfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		binfo->bmiHeader.biWidth = video->w;
		binfo->bmiHeader.biHeight = -video->h;	/* -ve for topdown bitmap */
		binfo->bmiHeader.biPlanes = 1;
		binfo->bmiHeader.biSizeImage = video->h * video->pitch;
		binfo->bmiHeader.biXPelsPerMeter = 0;
		binfo->bmiHeader.biYPelsPerMeter = 0;
		binfo->bmiHeader.biClrUsed = 0;
		binfo->bmiHeader.biClrImportant = 0;
		binfo->bmiHeader.biBitCount = video->format->BitsPerPixel;

		if ( is16bitmode ) {
			/* BI_BITFIELDS tells CreateDIBSection about the rgb masks in the palette */
			binfo->bmiHeader.biCompression = BI_BITFIELDS;
			((Uint32*)binfo->bmiColors)[0] = video->format->Rmask;
			((Uint32*)binfo->bmiColors)[1] = video->format->Gmask;
			((Uint32*)binfo->bmiColors)[2] = video->format->Bmask;
		} else {
#ifdef UNDER_CE
			binfo->bmiHeader.biCompression = BI_RGB;	/* 332 */
			if ( video->format->palette ) {
				binfo->bmiHeader.biClrUsed = video->format->palette->ncolors;
				for(i=0; i<video->format->palette->ncolors; i++)
				{
					binfo->bmiColors[i].rgbRed=i&(7<<5);
					binfo->bmiColors[i].rgbGreen=(i&(7<<2))<<3;
					binfo->bmiColors[i].rgbBlue=(i&3)<<5;
					binfo->bmiColors[i].rgbReserved=0;
			   }
			}
#else
			binfo->bmiHeader.biCompression = BI_RGB;	/* BI_BITFIELDS for 565 vs 555 */
			if ( video->format->palette ) {
				SDL_memset(binfo->bmiColors, 0,
					video->format->palette->ncolors*sizeof(RGBQUAD));
			}
#endif
		}

		/* Create the offscreen bitmap buffer */
		hdc = GetDC(SDL_Window);
		/* See if we need to rotate the buffer (WinCE specific) */
		screenWidth = GetDeviceCaps(hdc, HORZRES);
		screenHeight = GetDeviceCaps(hdc, VERTRES);
		this->hidden->orientation = SDL_ORIENTATION_UP;
		work_pixels = NULL;
		if (rotation_pixels) {
			free(rotation_pixels);
			rotation_pixels = NULL;
		}

		if ((flags & SDL_FULLSCREEN) && (width>height) && (width > screenWidth) ) {
			/* OK, we rotate the screen */
			video->pixels = malloc(video->h * video->pitch);
			rotation_pixels = video->pixels;
			if (video->pixels)
				this->hidden->orientation = SDL_ORIENTATION_LEFT;
			OutputDebugString(TEXT("will rotate\r\n"));
		}

		screen_bmp = CreateDIBSection(hdc, binfo, DIB_RGB_COLORS,
			(this->hidden->orientation == SDL_ORIENTATION_UP ? (void **)(&video->pixels) : (void**)&work_pixels), NULL, 0);
		ReleaseDC(SDL_Window, hdc);
#if defined(UNDER_CE) 
/* keep bitmapinfo for palette in 8-bit modes for devices that don't have SetDIBColorTable */
		last_bits = (this->hidden->orientation == SDL_ORIENTATION_UP ? (void **)(&video->pixels) : (void**)&work_pixels);
		if(last_bitmapinfo)
			free(last_bitmapinfo);
		if(is16bitmode)
		{
			last_bitmapinfo = 0;
			free(binfo);
		} else
			last_bitmapinfo = binfo;
#else
		SDL_free(binfo);
#endif
		if ( screen_bmp == NULL ) {
			if ( video != current ) {
				SDL_FreeSurface(video);
			}
			SDL_SetError("Couldn't create DIB section");
			return(NULL);
		}
		this->UpdateRects = (work_pixels ? DIB_RotatedUpdate : DIB_NormalUpdate);

		/* Set video surface flags */
		if ( bpp <= 8 ) {
			/* BitBlt() maps colors for us */
			video->flags |= SDL_HWPALETTE;
		}
	}
#ifndef _WIN32_WCE
	/* Resize the window */
	if ( !SDL_windowid && !IsZoomed(SDL_Window) ) {
#else
	if ( !SDL_windowid ) {
#endif
		HWND top;
		UINT swp_flags;
		const char *window = NULL;
		const char *center = NULL;

		if ( !SDL_windowX && !SDL_windowY ) {
			window = SDL_getenv("SDL_VIDEO_WINDOW_POS");
			center = SDL_getenv("SDL_VIDEO_CENTERED");
			if ( window ) {
				if ( SDL_sscanf(window, "%d,%d", &x, &y) == 2 ) {
					SDL_windowX = x;
					SDL_windowY = y;
				}
				if ( SDL_strcmp(window, "center") == 0 ) {
					center = window;
				}
			}
		}
		swp_flags = (SWP_NOCOPYBITS | SWP_SHOWWINDOW);

		bounds.left = SDL_windowX;
		bounds.top = SDL_windowY;
		bounds.right = SDL_windowX+video->w;
		bounds.bottom = SDL_windowY+video->h;
#ifdef UNDER_CE
		if(this->hidden->orientation != SDL_ORIENTATION_UP)
		{   
			int t=bounds.right;
			bounds.right = bounds.bottom;
			bounds.bottom=t;
		}
#endif
		AdjustWindowRectEx(&bounds, GetWindowLong(SDL_Window, GWL_STYLE), FALSE, 0);
		width = bounds.right-bounds.left;
		height = bounds.bottom-bounds.top;
		if ( (flags & SDL_FULLSCREEN) ) {
			x = (GetSystemMetrics(SM_CXSCREEN)-width)/2;
			y = (GetSystemMetrics(SM_CYSCREEN)-height)/2;
		} else if ( center ) {
			x = (GetSystemMetrics(SM_CXSCREEN)-width)/2;
			y = (GetSystemMetrics(SM_CYSCREEN)-height)/2;
		} else if ( SDL_windowX || SDL_windowY || window ) {
			x = bounds.left;
			y = bounds.top;
		} else {
			x = y = -1;
#ifndef UNDER_CE
			swp_flags |= SWP_NOMOVE;
#endif
		}
		if ( flags & SDL_FULLSCREEN ) {
			top = HWND_TOPMOST;
		} else {
			top = HWND_NOTOPMOST;
		}
#ifndef _WIN32_WCE
		SetWindowPos(SDL_Window, top, x, y, width, height, swp_flags);
		if ( !(flags & SDL_FULLSCREEN) ) {
			SDL_windowX = SDL_bounds.left;
			SDL_windowY = SDL_bounds.top;
		}
#else
		if (flags & SDL_FULLSCREEN) {
/* When WinCE program switches resolution from larger to smaller we should move its window so it would be visible in fullscreen */
//			SetWindowPos(SDL_Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			DIB_ShowTaskBar(FALSE);
			if(x>0) x=0;	// remove space from the left side of a screen in 320x200 mode
			if(y>0) y=0;
			SetWindowPos(SDL_Window, HWND_TOPMOST, x, y, width, height, SWP_NOCOPYBITS);
			ShowWindow(SDL_Window, SW_SHOW);
		}
		else
			SetWindowPos(SDL_Window, top, x, y, width, height, swp_flags);
#endif
		
		SDL_resizing = 0;
		SetForegroundWindow(SDL_Window);
	}
	SDL_resizing = 0;

	/* Set up for OpenGL */
	if ( flags & SDL_OPENGL ) {
#ifdef HAVE_OPENGL
		if ( WIN_GL_SetupWindow(this) < 0 ) {
			return(NULL);
		}
		video->flags |= SDL_OPENGL;
#else
		return (NULL);
#endif

	}

	/* JC 14 Mar 2006
		Flush the message loop or this can cause big problems later
		Especially if the user decides to use dialog boxes or assert()!
	*/
	WIN_FlushMessageQueue();
#ifdef ENABLE_WINGAPI
	/* Grab hardware keys if necessary */
	if ( flags & SDL_FULLSCREEN ) {
		GAPI_GrabHardwareKeys(TRUE);
	}
#endif

	/* We're live! */
	return(video);
}

/* We don't actually allow hardware surfaces in the DIB driver */
static int DIB_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void DIB_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}
static int DIB_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0); 
}
static void DIB_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static inline void rotateBlit(unsigned short *src, unsigned short *dest, SDL_Rect *rect, int pitch) {
	int i=rect->w, j=rect->h;
	src+=i;

	for (;i--;) {
		register unsigned short *S=src--;
// I use loop unrolling to spedup things a little
		int cnt = j;
		if(cnt&1)
		{
			*(dest++) = *S;
			S+=pitch;
		}
		cnt>>=1;
		if(cnt&1)
		{
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
		}
		cnt>>=1;
		for (; cnt--; ) {
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
		}
	}
/* tiny optimization
	int i, j;
	src+=rect->w;

	for (i=0; i<rect->w; i++) {
		register unsigned short *S=src--;
		for (j=0; j<rect->h; j++) {
			*(dest++) = *S;
			S+=pitch;
		}
	}
*/
/* original unoptimized version
	int i, j;

	for (i=0; i<rect->w; i++) {
		for (j=0; j<rect->h; j++) {
			dest[i * rect->h + j] = src[pitch * j + (rect->w - i)];
		}
	}
*/
}

static inline void rotateBlit8(unsigned char *src, unsigned char *dest, SDL_Rect *rect, int pitch) {
	int i=rect->w, j=rect->h;
	src+=i;

	for (;i--;) {
		register unsigned char *S=src--;
// I use loop unrolling to spedup things a little
		int cnt = j;
		if(cnt&1)
		{
			*(dest++) = *S;
			S+=pitch;
		}
		cnt>>=1;
		if(cnt&1)
		{
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
		}
		cnt>>=1;
		for (; cnt--; ) {
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
			*(dest++) = *S;
			S+=pitch;
		}
	}
}

static void DIB_RotatedUpdate(_THIS, int numrects, SDL_Rect *rects) 
{
	HDC hdc, mdc;
	HBITMAP hb, old;
	int i;

	hdc = GetDC(SDL_Window);
	if ( screen_pal ) {
		SelectPalette(hdc, screen_pal, FALSE);
	}
	mdc = CreateCompatibleDC(hdc);
	/*SelectObject(mdc, screen_bmp);*/
	if(this->screen->format->BytesPerPixel == 2) {
		for ( i=0; i<numrects; ++i ) {	
			unsigned short *src = (unsigned short*)this->screen->pixels;
			rotateBlit(src + (this->screen->w * rects[i].y) + rects[i].x, work_pixels, &rects[i], this->screen->w);		
			hb = CreateBitmap(rects[i].h, rects[i].w, 1, 16, work_pixels);
			old = (HBITMAP)SelectObject(mdc, hb);
			BitBlt(hdc, rects[i].y, this->screen->w - (rects[i].x + rects[i].w), rects[i].h, rects[i].w,
					mdc, 0, 0, SRCCOPY);
			SelectObject(mdc, old);
			DeleteObject(hb);
		}
	} else {
		if ( screen_pal ) {
			SelectPalette(mdc, screen_pal, FALSE);
		}
		for ( i=0; i<numrects; ++i ) {	
			unsigned char *src = (unsigned char*)this->screen->pixels;
			rotateBlit8(src + (this->screen->w * rects[i].y) + rects[i].x, work_pixels, &rects[i], this->screen->w);
			hb = CreateBitmap(rects[i].h, rects[i].w, 1, 8, work_pixels);
			old = (HBITMAP)SelectObject(mdc, hb);
			BitBlt(hdc, rects[i].y, this->screen->w - (rects[i].x + rects[i].w), rects[i].h, rects[i].w,
					mdc, 0, 0, SRCCOPY);
			SelectObject(mdc, old);
			DeleteObject(hb); 
		}
	}
	DeleteDC(mdc);
	ReleaseDC(SDL_Window, hdc);
}

static void DIB_NormalUpdate(_THIS, int numrects, SDL_Rect *rects)
{
	HDC hdc, mdc;
	int i;
	HBITMAP old;

	hdc = GetDC(SDL_Window);
	if ( screen_pal ) {
		SelectPalette(hdc, screen_pal, FALSE);
	}
	mdc = CreateCompatibleDC(hdc);
	old = (HBITMAP)SelectObject(mdc, screen_bmp);
	for ( i=0; i<numrects; ++i ) {
		BitBlt(hdc, rects[i].x, rects[i].y, rects[i].w, rects[i].h,
					mdc, rects[i].x, rects[i].y, SRCCOPY);
	}
	SelectObject(mdc, old);
	DeleteDC(mdc);
	ReleaseDC(SDL_Window, hdc);
}


int DIB_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)
	HDC hdc, mdc;
	RGBQUAD *pal;
#else
	HDC hdc;
#endif
	int i;

#if defined(UNDER_CE) && defined(NO_SETDIBCOLORTABLE)
	if(last_bitmapinfo==0)
		return 0;
#endif

	/* Update the display palette */
	hdc = GetDC(SDL_Window);
	if ( screen_pal ) {
		PALETTEENTRY *entries;

		entries = SDL_stack_alloc(PALETTEENTRY, ncolors);
		for ( i=0; i<ncolors; ++i ) {
			entries[i].peRed   = colors[i].r;
			entries[i].peGreen = colors[i].g;
			entries[i].peBlue  = colors[i].b;
			entries[i].peFlags = PC_NOCOLLAPSE;
		}
		SetPaletteEntries(screen_pal, firstcolor, ncolors, entries);
		SelectPalette(hdc, screen_pal, FALSE);
		RealizePalette(hdc);
		SDL_stack_free(entries);
	}

#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)
	/* Copy palette colors into DIB palette */
	pal = SDL_stack_alloc(RGBQUAD, ncolors);
	for ( i=0; i<ncolors; ++i ) {
		pal[i].rgbRed = colors[i].r;
		pal[i].rgbGreen = colors[i].g;
		pal[i].rgbBlue = colors[i].b;
		pal[i].rgbReserved = 0;
	}

	/* Set the DIB palette and update the display */
	mdc = CreateCompatibleDC(hdc);

#if defined(UNDER_CE) 
#if !defined(NO_SETDIBCOLORTABLE)
/* BUG: For some reason SetDIBColorTable is not working when screen is not rotated */
	if(rotation == SDL_ORIENTATION_UP && last_bitmapinfo)
#else
	if(1)
#endif
	{
		DeleteObject(screen_bmp);
		last_bitmapinfo->bmiHeader.biClrUsed=256;
		for ( i=firstcolor; i<firstcolor+ncolors; ++i )
			last_bitmapinfo->bmiColors[i]=pal[i];
		screen_bmp = CreateDIBSection(hdc, last_bitmapinfo, DIB_RGB_COLORS,
			last_bits, NULL, 0);
    }
#else
	SelectObject(mdc, screen_bmp);
	SetDIBColorTable(mdc, firstcolor, ncolors, pal);
#endif
#ifndef UNDER_CE
	BitBlt(hdc, 0, 0, this->screen->w, this->screen->h,
	       mdc, 0, 0, SRCCOPY);
	DeleteDC(mdc);
	SDL_stack_free(pal);
#else
	{
		SDL_Rect rect;
		rect.x=0; rect.y=0;
		rect.w=this->screen->w; rect.h=this->screen->h;
// Fixme: screen flickers:		(this->UpdateRects)(this, 1, &rect) ;
	}
#endif
	DeleteDC(mdc);
#endif
	ReleaseDC(SDL_Window, hdc);
	return(1);
}


static void DIB_CheckGamma(_THIS)
{
#ifndef NO_GAMMA_SUPPORT
	HDC hdc;
	WORD ramp[3*256];

	/* If we fail to get gamma, disable gamma control */
	hdc = GetDC(SDL_Window);
	if ( ! GetDeviceGammaRamp(hdc, ramp) ) {
		this->GetGammaRamp = NULL;
		this->SetGammaRamp = NULL;
	}
	ReleaseDC(SDL_Window, hdc);
#endif /* !NO_GAMMA_SUPPORT */
}
void DIB_SwapGamma(_THIS)
{
#ifndef NO_GAMMA_SUPPORT
	HDC hdc;

	if ( gamma_saved ) {
		hdc = GetDC(SDL_Window);
		if ( SDL_GetAppState() & SDL_APPINPUTFOCUS ) {
			/* About to leave active state, restore gamma */
			SetDeviceGammaRamp(hdc, gamma_saved);
		} else {
			/* About to enter active state, set game gamma */
			GetDeviceGammaRamp(hdc, gamma_saved);
			SetDeviceGammaRamp(hdc, this->gamma);
		}
		ReleaseDC(SDL_Window, hdc);
	}
#endif /* !NO_GAMMA_SUPPORT */
}
void DIB_QuitGamma(_THIS)
{
#ifndef NO_GAMMA_SUPPORT
	if ( gamma_saved ) {
		/* Restore the original gamma if necessary */
		if ( SDL_GetAppState() & SDL_APPINPUTFOCUS ) {
			HDC hdc;

			hdc = GetDC(SDL_Window);
			SetDeviceGammaRamp(hdc, gamma_saved);
			ReleaseDC(SDL_Window, hdc);
		}

		/* Free the saved gamma memory */
		SDL_free(gamma_saved);
		gamma_saved = 0;
	}
#endif /* !NO_GAMMA_SUPPORT */
}

int DIB_SetGammaRamp(_THIS, Uint16 *ramp)
{
#ifdef NO_GAMMA_SUPPORT
	SDL_SetError("SDL compiled without gamma ramp support");
	return -1;
#else
	HDC hdc;
	BOOL succeeded;

	/* Set the ramp for the display */
	if ( ! gamma_saved ) {
		gamma_saved = (WORD *)SDL_malloc(3*256*sizeof(*gamma_saved));
		if ( ! gamma_saved ) {
			SDL_OutOfMemory();
			return -1;
		}
		hdc = GetDC(SDL_Window);
		GetDeviceGammaRamp(hdc, gamma_saved);
		ReleaseDC(SDL_Window, hdc);
	}
	if ( SDL_GetAppState() & SDL_APPINPUTFOCUS ) {
		hdc = GetDC(SDL_Window);
		succeeded = SetDeviceGammaRamp(hdc, ramp);
		ReleaseDC(SDL_Window, hdc);
	} else {
		succeeded = TRUE;
	}
	return succeeded ? 0 : -1;
#endif /* !NO_GAMMA_SUPPORT */
}

int DIB_GetGammaRamp(_THIS, Uint16 *ramp)
{
#ifdef NO_GAMMA_SUPPORT
	SDL_SetError("SDL compiled without gamma ramp support");
	return -1;
#else
	HDC hdc;
	BOOL succeeded;

	/* Get the ramp from the display */
	hdc = GetDC(SDL_Window);
	succeeded = GetDeviceGammaRamp(hdc, ramp);
	ReleaseDC(SDL_Window, hdc);
	return succeeded ? 0 : -1;
#endif /* !NO_GAMMA_SUPPORT */
}

void DIB_VideoQuit(_THIS)
{
	/* Destroy the window and everything associated with it */
	DIB_ShowTaskBar(TRUE);
#ifdef ENABLE_WINGAPI
	GAPI_GrabHardwareKeys(FALSE);
#endif

	if ( SDL_Window ) {
		/* Delete the screen bitmap (also frees screen->pixels) */
		if ( this->screen ) {
#ifndef NO_CHANGEDISPLAYSETTINGS
			if ( this->screen->flags & SDL_FULLSCREEN ) {
				ChangeDisplaySettings(NULL, 0);
				ShowWindow(SDL_Window, SW_HIDE);
			}
#endif

#ifdef HAVE_OPENGL
			if ( this->screen->flags & SDL_OPENGL ) {
				WIN_GL_ShutDown(this);
			}
#endif

			this->screen->pixels = NULL;
		}
		if ( screen_bmp ) {
			DeleteObject(screen_bmp);
			screen_bmp = NULL;
		}
		if ( screen_icn ) {
			DestroyIcon(screen_icn);
			screen_icn = NULL;
		}
		DIB_QuitGamma(this);
		DIB_DestroyWindow(this);

		SDL_Window = NULL;

#if defined(_WIN32_WCE)

// Unload wince aygshell library to prevent leak
		if( aygshell ) 
		{
			FreeLibrary(aygshell);
			aygshell = NULL;
		}
#endif

	}
}

/* Exported for the windows message loop only */
static void DIB_FocusPalette(_THIS, int foreground)
{
	if ( screen_pal != NULL ) {
		HDC hdc;

		hdc = GetDC(SDL_Window);
		SelectPalette(hdc, screen_pal, FALSE);
		if ( RealizePalette(hdc) )
			InvalidateRect(SDL_Window, NULL, FALSE);
		ReleaseDC(SDL_Window, hdc);
	}
}
static void DIB_RealizePalette(_THIS)
{
	DIB_FocusPalette(this, 1);
}
static void DIB_PaletteChanged(_THIS, HWND window)
{
	if ( window != SDL_Window ) {
		DIB_FocusPalette(this, 0);
	}
}

/* Exported for the windows message loop only */
static void DIB_WinPAINT(_THIS, HDC hdc)
{
	HDC mdc;

	if ( screen_pal ) {
		SelectPalette(hdc, screen_pal, FALSE);
	}
	mdc = CreateCompatibleDC(hdc);
	SelectObject(mdc, screen_bmp);
	BitBlt(hdc, 0, 0, SDL_VideoSurface->w, SDL_VideoSurface->h,
							mdc, 0, 0, SRCCOPY);
	DeleteDC(mdc);
}

/* Stub in case DirectX isn't available */
#if !SDL_AUDIO_DRIVER_DSOUND
void DX5_SoundFocus(HWND hwnd)
{
	return;
}
#endif
