/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2004 Sam Lantinga

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
 "@(#) $Id$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#include "SDL_events.h"
#include "SDL_error.h"
#include "SDL_syswm.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_lowvideo.h"
#include "SDL_dibvideo.h"
#include "SDL_vkeys.h"

#ifndef WM_APP
#define WM_APP	0x8000
#endif

#ifdef _WIN32_WCE
#define NO_GETKEYBOARDSTATE
#endif

/* The translation table from a Microsoft VK keysym to a SDL keysym */
static SDLKey VK_keymap[SDLK_LAST];
static SDL_keysym *TranslateKey(UINT vkey, UINT scancode, SDL_keysym *keysym, int pressed);
static BOOL prev_shiftstates[2];

/* Masks for processing the windows KEYDOWN and KEYUP messages */
#define REPEATED_KEYMASK	(1<<30)
#define EXTENDED_KEYMASK	(1<<24)

/* DJM: If the user setup the window for us, we want to save his window proc,
   and give him a chance to handle some messages. */
static WNDPROC userWindowProc = NULL;

#ifdef _WIN32_WCE

WPARAM rotateKey(WPARAM key, SDL_RotateAttr direction) {
	switch (direction) {
		case SDL_ROTATE_NONE:
			return key;

		case SDL_ROTATE_LEFT:
			switch (key) {
				case VK_UP:
					return VK_RIGHT;
				case VK_RIGHT:
					return VK_DOWN;
				case VK_DOWN:
					return VK_LEFT;
				case VK_LEFT:
					return VK_UP;
			}

		case SDL_ROTATE_RIGHT:
			switch (key) {
				case VK_UP:
					return VK_LEFT;
				case VK_RIGHT:
					return VK_UP;
				case VK_DOWN:
					return VK_RIGHT;
				case VK_LEFT:
					return VK_DOWN;
			}
	}

	return key;
}

#endif


/* The main Win32 event handler */
LONG
 DIB_HandleMessage(_THIS, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	extern int posted;
	MSG tmpmsg;
	static int dropnextvklwinup = 0;

	switch (msg) {
		//case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			SDL_keysym keysym;

#ifdef _WIN32_WCE
			// Drop spurious keystrokes
			if ( 	wParam == VK_F20 ||					// VK_ROCKER with arrow keys
				wParam == VK_F21 ||					// VK_DPAD with arrow keys
				wParam == VK_F23					// VK_ACTION with dpad enter
			   )
				return 0;

			if (wParam == VK_LWIN && PeekMessage(&tmpmsg, NULL, 0, 0, PM_NOREMOVE))		// drop VK_LWIN messages if they're part of a chord
				if (tmpmsg.message == WM_KEYDOWN && tmpmsg.wParam != VK_LWIN) {
#ifdef WMMSG_DEBUG
					debugLog("SDL: DROPPING chorded VK_LWIN DOWN");
#endif
					dropnextvklwinup = 1;
					return 0;
				}

			// Rotate key if necessary
			if (rotation != SDL_ROTATE_NONE)
				wParam = rotateKey(wParam, rotation);	
#endif

			/* Ignore repeated keys */
			if ( lParam&REPEATED_KEYMASK ) {
				return(0);
			}
			switch (wParam) {
				case VK_CONTROL:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RCONTROL;
					else
						wParam = VK_LCONTROL;
					break;
				case VK_SHIFT:
					/* EXTENDED trick doesn't work here */
					if (!prev_shiftstates[0] && (GetKeyState(VK_LSHIFT) & 0x8000)) {
						wParam = VK_LSHIFT;
						prev_shiftstates[0] = TRUE;
					} else if (!prev_shiftstates[1] && (GetKeyState(VK_RSHIFT) & 0x8000)) {
						wParam = VK_RSHIFT;
						prev_shiftstates[1] = TRUE;
					} else {
						/* Huh? */
					}
					break;
				case VK_MENU:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RMENU;
					else
						wParam = VK_LMENU;
					break;
			}
#ifdef _WIN32_WCE
			TranslateKey(wParam,HIWORD(lParam),&keysym,1);
			if (keysym.sym != SDLK_UNKNOWN)					// drop keys w/ 0 keycode
				posted = SDL_PrivateKeyboard(SDL_PRESSED, &keysym);
#ifdef WMMSG_DEBUG
			else
				debugLog("SDL: DROPPING SDLK_UNKNOWN DOWN");
#endif
#else
			posted = SDL_PrivateKeyboard(SDL_PRESSED,TranslateKey(wParam,HIWORD(lParam),&keysym,1));
#endif
		}
		return(0);

		//case WM_SYSKEYUP:
		case WM_KEYUP: {
			SDL_keysym keysym;

#ifdef _WIN32_WCE
			// Drop spurious keystrokes
			if ( 	wParam == VK_F20 ||					// VK_ROCKER with arrow keys
				wParam == VK_F21 ||					// VK_DPAD with arrow keys
				wParam == VK_F23					// VK_ACTION with dpad enter
			   )
				return 0;

			if (dropnextvklwinup && wParam == VK_LWIN) {			// drop VK_LWIN messages if they're part of a chord
#ifdef WMMSG_DEBUG
				debugLog("SDL: DROPPING chorded VK_LWIN UP");
#endif
				dropnextvklwinup = 0;
				return 0;
			}

			// Rotate key if necessary
			if (rotation != SDL_ROTATE_NONE)
				wParam = rotateKey(wParam, rotation);
#endif

			switch (wParam) {
				case VK_CONTROL:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RCONTROL;
					else
						wParam = VK_LCONTROL;
					break;
				case VK_SHIFT:
					/* EXTENDED trick doesn't work here */
					if (prev_shiftstates[0] && !(GetKeyState(VK_LSHIFT) & 0x8000)) {
						wParam = VK_LSHIFT;
						prev_shiftstates[0] = FALSE;
					} else if (prev_shiftstates[1] && !(GetKeyState(VK_RSHIFT) & 0x8000)) {
						wParam = VK_RSHIFT;
						prev_shiftstates[1] = FALSE;
					} else {
						/* Huh? */
					}
					break;
				case VK_MENU:
					if ( lParam&EXTENDED_KEYMASK )
						wParam = VK_RMENU;
					else
						wParam = VK_LMENU;
					break;
			}
#ifdef _WIN32_WCE
			TranslateKey(wParam,HIWORD(lParam),&keysym,0);
			if (keysym.sym != SDLK_UNKNOWN)					// drop keys w/ 0 keycode
				posted = SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
#ifdef WMMSG_DEBUG
			else
				debugLog("SDL: DROPPING SDLK_UNKNOWN UP");
#endif
#else
			posted = SDL_PrivateKeyboard(SDL_RELEASED,TranslateKey(wParam,HIWORD(lParam),&keysym,0));
#endif
		}
		return(0);

#if defined(SC_SCREENSAVE) && defined(SC_MONITORPOWER)
		case WM_SYSCOMMAND: {
			if ((wParam&0xFFF0)==SC_SCREENSAVE ||
				(wParam&0xFFF0)==SC_MONITORPOWER)
					return(0);
		}
		/* Fall through to default processing */
#endif /* SC_SCREENSAVE && SC_MONITORPOWER */

		default: {
			/* Only post the event if we're watching for it */
			if ( SDL_ProcessEvents[SDL_SYSWMEVENT] == SDL_ENABLE ) {
			        SDL_SysWMmsg wmmsg;

				SDL_VERSION(&wmmsg.version);
				wmmsg.hwnd = hwnd;
				wmmsg.msg = msg;
				wmmsg.wParam = wParam;
				wmmsg.lParam = lParam;
				posted = SDL_PrivateSysWMEvent(&wmmsg);

			/* DJM: If the user isn't watching for private
				messages in her SDL event loop, then pass it
				along to any win32 specific window proc.
			 */
			} else if (userWindowProc) {
				return CallWindowProc(userWindowProc, hwnd, msg, wParam, lParam);
			}
		}
		break;
	}
	return(DefWindowProc(hwnd, msg, wParam, lParam));
}

void DIB_PumpEvents(_THIS)
{
	MSG msg;

	while ( PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) ) {
		if ( GetMessage(&msg, NULL, 0, 0) > 0 ) {
			DispatchMessage(&msg);
		}
	}
}

void DIB_InitOSKeymap(_THIS)
{
	int i;

	/* Map the VK keysyms */
	for ( i=0; i<SDL_TABLESIZE(VK_keymap); ++i )
		VK_keymap[i] = SDLK_UNKNOWN;

	VK_keymap[VK_BACK] = SDLK_BACKSPACE;
	VK_keymap[VK_TAB] = SDLK_TAB;
	VK_keymap[VK_CLEAR] = SDLK_CLEAR;
	VK_keymap[VK_RETURN] = SDLK_RETURN;
	VK_keymap[VK_PAUSE] = SDLK_PAUSE;
	VK_keymap[VK_ESCAPE] = SDLK_ESCAPE;
	VK_keymap[VK_SPACE] = SDLK_SPACE;
	VK_keymap[VK_APOSTROPHE] = SDLK_QUOTE;
	VK_keymap[VK_COMMA] = SDLK_COMMA;
	VK_keymap[VK_MINUS] = SDLK_MINUS;
	VK_keymap[VK_PERIOD] = SDLK_PERIOD;
	VK_keymap[VK_SLASH] = SDLK_SLASH;
	VK_keymap[VK_0] = SDLK_0;
	VK_keymap[VK_1] = SDLK_1;
	VK_keymap[VK_2] = SDLK_2;
	VK_keymap[VK_3] = SDLK_3;
	VK_keymap[VK_4] = SDLK_4;
	VK_keymap[VK_5] = SDLK_5;
	VK_keymap[VK_6] = SDLK_6;
	VK_keymap[VK_7] = SDLK_7;
	VK_keymap[VK_8] = SDLK_8;
	VK_keymap[VK_9] = SDLK_9;
	VK_keymap[VK_SEMICOLON] = SDLK_SEMICOLON;
	VK_keymap[VK_EQUALS] = SDLK_EQUALS;
	VK_keymap[VK_LBRACKET] = SDLK_LEFTBRACKET;
	VK_keymap[VK_BACKSLASH] = SDLK_BACKSLASH;
	VK_keymap[VK_RBRACKET] = SDLK_RIGHTBRACKET;
	VK_keymap[VK_GRAVE] = SDLK_BACKQUOTE;
	VK_keymap[VK_BACKTICK] = SDLK_BACKQUOTE;
#ifndef _WIN32_WCE
	VK_keymap[VK_A] = SDLK_a;
	VK_keymap[VK_B] = SDLK_b;
	VK_keymap[VK_C] = SDLK_c;
	VK_keymap[VK_D] = SDLK_d;
	VK_keymap[VK_E] = SDLK_e;
	VK_keymap[VK_F] = SDLK_f;
	VK_keymap[VK_G] = SDLK_g;
	VK_keymap[VK_H] = SDLK_h;
	VK_keymap[VK_I] = SDLK_i;
	VK_keymap[VK_J] = SDLK_j;
	VK_keymap[VK_K] = SDLK_k;
	VK_keymap[VK_L] = SDLK_l;
	VK_keymap[VK_M] = SDLK_m;
	VK_keymap[VK_N] = SDLK_n;
	VK_keymap[VK_O] = SDLK_o;
	VK_keymap[VK_P] = SDLK_p;
	VK_keymap[VK_Q] = SDLK_q;
	VK_keymap[VK_R] = SDLK_r;
	VK_keymap[VK_S] = SDLK_s;
	VK_keymap[VK_T] = SDLK_t;
	VK_keymap[VK_U] = SDLK_u;
	VK_keymap[VK_V] = SDLK_v;
	VK_keymap[VK_W] = SDLK_w;
	VK_keymap[VK_X] = SDLK_x;
	VK_keymap[VK_Y] = SDLK_y;
	VK_keymap[VK_Z] = SDLK_z;
#else
	VK_keymap['A'] = SDLK_a;
	VK_keymap['B'] = SDLK_b;
	VK_keymap['C'] = SDLK_c;
	VK_keymap['D'] = SDLK_d;
	VK_keymap['E'] = SDLK_e;
	VK_keymap['F'] = SDLK_f;
	VK_keymap['G'] = SDLK_g;
	VK_keymap['H'] = SDLK_h;
	VK_keymap['I'] = SDLK_i;
	VK_keymap['J'] = SDLK_j;
	VK_keymap['K'] = SDLK_k;
	VK_keymap['L'] = SDLK_l;
	VK_keymap['M'] = SDLK_m;
	VK_keymap['N'] = SDLK_n;
	VK_keymap['O'] = SDLK_o;
	VK_keymap['P'] = SDLK_p;
	VK_keymap['Q'] = SDLK_q;
	VK_keymap['R'] = SDLK_r;
	VK_keymap['S'] = SDLK_s;
	VK_keymap['T'] = SDLK_t;
	VK_keymap['U'] = SDLK_u;
	VK_keymap['V'] = SDLK_v;
	VK_keymap['W'] = SDLK_w;
	VK_keymap['X'] = SDLK_x;
	VK_keymap['Y'] = SDLK_y;
	VK_keymap['Z'] = SDLK_z;
#endif
	VK_keymap[VK_DELETE] = SDLK_DELETE;

	VK_keymap[VK_NUMPAD0] = SDLK_KP0;
	VK_keymap[VK_NUMPAD1] = SDLK_KP1;
	VK_keymap[VK_NUMPAD2] = SDLK_KP2;
	VK_keymap[VK_NUMPAD3] = SDLK_KP3;
	VK_keymap[VK_NUMPAD4] = SDLK_KP4;
	VK_keymap[VK_NUMPAD5] = SDLK_KP5;
	VK_keymap[VK_NUMPAD6] = SDLK_KP6;
	VK_keymap[VK_NUMPAD7] = SDLK_KP7;
	VK_keymap[VK_NUMPAD8] = SDLK_KP8;
	VK_keymap[VK_NUMPAD9] = SDLK_KP9;
	VK_keymap[VK_DECIMAL] = SDLK_KP_PERIOD;
	VK_keymap[VK_DIVIDE] = SDLK_KP_DIVIDE;
	VK_keymap[VK_MULTIPLY] = SDLK_KP_MULTIPLY;
	VK_keymap[VK_SUBTRACT] = SDLK_KP_MINUS;
	VK_keymap[VK_ADD] = SDLK_KP_PLUS;

	VK_keymap[VK_UP] = SDLK_UP;
	VK_keymap[VK_DOWN] = SDLK_DOWN;
	VK_keymap[VK_RIGHT] = SDLK_RIGHT;
	VK_keymap[VK_LEFT] = SDLK_LEFT;
	VK_keymap[VK_INSERT] = SDLK_INSERT;
	VK_keymap[VK_HOME] = SDLK_HOME;
	VK_keymap[VK_END] = SDLK_END;
	VK_keymap[VK_PRIOR] = SDLK_PAGEUP;
	VK_keymap[VK_NEXT] = SDLK_PAGEDOWN;

	VK_keymap[VK_F1] = SDLK_F1;
	VK_keymap[VK_F2] = SDLK_F2;
	VK_keymap[VK_F3] = SDLK_F3;
	VK_keymap[VK_F4] = SDLK_F4;
	VK_keymap[VK_F5] = SDLK_F5;
	VK_keymap[VK_F6] = SDLK_F6;
	VK_keymap[VK_F7] = SDLK_F7;
	VK_keymap[VK_F8] = SDLK_F8;
	VK_keymap[VK_F9] = SDLK_F9;
	VK_keymap[VK_F10] = SDLK_F10;
	VK_keymap[VK_F11] = SDLK_F11;
	VK_keymap[VK_F12] = SDLK_F12;
	VK_keymap[VK_F13] = SDLK_F13;
	VK_keymap[VK_F14] = SDLK_F14;
	VK_keymap[VK_F15] = SDLK_F15;
	VK_keymap[VK_F16] = SDLK_F16;
	VK_keymap[VK_F17] = SDLK_F17;
	VK_keymap[VK_F18] = SDLK_F18;
	VK_keymap[VK_F19] = SDLK_F19;
	VK_keymap[VK_F20] = SDLK_F20;
	VK_keymap[VK_F21] = SDLK_F21;
	VK_keymap[VK_F22] = SDLK_F22;
	VK_keymap[VK_F23] = SDLK_F23;
	VK_keymap[VK_F24] = SDLK_F24;

	VK_keymap[VK_NUMLOCK] = SDLK_NUMLOCK;
	VK_keymap[VK_CAPITAL] = SDLK_CAPSLOCK;
	VK_keymap[VK_SCROLL] = SDLK_SCROLLOCK;
	VK_keymap[VK_RSHIFT] = SDLK_RSHIFT;
	VK_keymap[VK_LSHIFT] = SDLK_LSHIFT;
	VK_keymap[VK_RCONTROL] = SDLK_RCTRL;
	VK_keymap[VK_LCONTROL] = SDLK_LCTRL;
	VK_keymap[VK_RMENU] = SDLK_RALT;
	VK_keymap[VK_LMENU] = SDLK_LALT;
	VK_keymap[VK_RWIN] = SDLK_RSUPER;
	VK_keymap[VK_LWIN] = SDLK_LSUPER;

	VK_keymap[VK_HELP] = SDLK_HELP;
#ifdef VK_PRINT
	VK_keymap[VK_PRINT] = SDLK_PRINT;
#endif
	VK_keymap[VK_SNAPSHOT] = SDLK_PRINT;
	VK_keymap[VK_CANCEL] = SDLK_BREAK;
	VK_keymap[VK_APPS] = SDLK_MENU;

	prev_shiftstates[0] = FALSE;
	prev_shiftstates[1] = FALSE;

#ifdef _WIN32_WCE
	VK_keymap[0xC1] = SDLK_APP1;
	VK_keymap[0xC2] = SDLK_APP2;
	VK_keymap[0xC3] = SDLK_APP3;
	VK_keymap[0xC4] = SDLK_APP4;
	VK_keymap[0xC5] = SDLK_APP5;
	VK_keymap[0xC6] = SDLK_APP6;
#endif
}

static SDL_keysym *TranslateKey(UINT vkey, UINT scancode, SDL_keysym *keysym, int pressed)
{
	/* Set the keysym information */
	keysym->scancode = (unsigned char) scancode;
	keysym->sym = VK_keymap[vkey];
	keysym->mod = KMOD_NONE;
	keysym->unicode = 0;
	if ( pressed && SDL_TranslateUNICODE ) { /* Someday use ToUnicode() */
#ifdef NO_GETKEYBOARDSTATE
		/* Uh oh, better hope the vkey is close enough.. */
		keysym->unicode = vkey;
#else
		BYTE keystate[256];
		BYTE chars[2];

		GetKeyboardState(keystate);
		if ( ToAscii(vkey,scancode,keystate,(WORD *)chars,0) == 1 ) {
			keysym->unicode = chars[0];
		}
#endif /* NO_GETKEYBOARDSTATE */
	}
	return(keysym);
}

int DIB_CreateWindow(_THIS)
{
#ifndef CS_BYTEALIGNCLIENT
#define CS_BYTEALIGNCLIENT	0
#endif
#ifndef _WIN32_WCE
	SDL_RegisterApp("SDL_app", CS_BYTEALIGNCLIENT, 0);
#else
	//SDL_RegisterApp("SDL_app", CS_BYTEALIGNCLIENT | CS_HREDRAW | CS_VREDRAW, 0);
	SDL_RegisterApp("SDL_app", CS_HREDRAW | CS_VREDRAW, 0);
#endif
	if ( SDL_windowid ) {
// FIXME 
#ifndef _WIN32_WCE
		SDL_Window = (HWND)strtol(SDL_windowid, NULL, 0);
		if ( SDL_Window == NULL ) {
			SDL_SetError("Couldn't get user specified window");
			return(-1);
		}

		/* DJM: we want all event's for the user specified
			window to be handled by SDL.
		 */
		userWindowProc = (WNDPROC)GetWindowLong(SDL_Window, GWL_WNDPROC);
		SetWindowLong(SDL_Window, GWL_WNDPROC, (LONG)WinMessage);
#endif
	} else {
				
#ifndef _WIN32_WCE
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname,
                        (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX),
                        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, SDL_Instance, NULL);
#else		
		SDL_Window = CreateWindow(SDL_Appname, SDL_Appname, WS_VISIBLE | WS_POPUP,
						0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 
						NULL, NULL, SDL_Instance, NULL);
		SetWindowPos(SDL_Window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

#endif	
		if ( SDL_Window == NULL ) {
			SDL_SetError("Couldn't create window");
			return(-1);
		}
#ifndef _WIN32_WCE
		ShowWindow(SDL_Window, SW_HIDE);		
#endif
	}

	return(0);
}

void DIB_DestroyWindow(_THIS)
{
	if ( SDL_windowid ) {
		SetWindowLong(SDL_Window, GWL_WNDPROC, (LONG)userWindowProc);
	} else {
		DestroyWindow(SDL_Window);
	}
}
