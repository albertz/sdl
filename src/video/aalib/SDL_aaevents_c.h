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

#include "SDL_aavideo.h"

/* Variables and functions exported by SDL_sysevents.c to other parts 
   of the native video subsystem (SDL_sysvideo.c)
*/
extern void AA_initkeymaps(int fd);
extern void AA_mousecallback(int button, int dx, int dy,
                                 int u1,int u2,int u3, int u4);
extern void AA_keyboardcallback(int scancode, int pressed);

extern void AA_InitOSKeymap(_THIS);
extern void AA_PumpEvents(_THIS);
