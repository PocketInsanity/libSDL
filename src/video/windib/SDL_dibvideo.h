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

#ifndef _SDL_dibvideo_h
#define _SDL_dibvideo_h

#include <windows.h>

/* Rotation direction */
typedef enum {
	SDL_ROTATE_NONE,
	SDL_ROTATE_LEFT,
	SDL_ROTATE_RIGHT
} SDL_RotateAttr;

/* Private display data */
struct SDL_PrivateVideoData {
    HBITMAP screen_bmp;
    HPALETTE screen_pal;
	void *work_pixels; /* if the display needs to be rotated, memory allocated by the API */
	void *rotation_pixels; /* if the display needs to be rotated, memory allocated by the code */
	SDL_RotateAttr rotation;
	char ozoneHack; /* force stylus translation if running without Hi Res flag */

#define NUM_MODELISTS	4		/* 8, 16, 24, and 32 bits-per-pixel */
    int SDL_nummodes[NUM_MODELISTS];
    SDL_Rect **SDL_modelist[NUM_MODELISTS];
};
/* Old variable names */
#define screen_bmp			(this->hidden->screen_bmp)
#define screen_pal			(this->hidden->screen_pal)
#define SDL_nummodes		(this->hidden->SDL_nummodes)
#define SDL_modelist		(this->hidden->SDL_modelist)
#define work_pixels			(this->hidden->work_pixels)
#define rotation			(this->hidden->rotation)
#define rotation_pixels		(this->hidden->rotation_pixels)
#define ozoneHack			(this->hidden->ozoneHack)

#endif /* _SDL_dibvideo_h */
