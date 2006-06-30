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

/* Useful functions and variables from SDL_events.c */
#include "SDL_events.h"
#include "SDL_mouse_c.h"
#include "SDL_keyboard_c.h"

/* Start and stop the event processing loop */
extern int SDL_StartEventLoop(Uint32 flags);
extern void SDL_StopEventLoop(void);
extern void SDL_QuitInterrupt(void);

extern void SDL_Lock_EventThread(void);
extern void SDL_Unlock_EventThread(void);
extern Uint32 SDL_EventThreadID(void);

extern int SDL_QuitInit(void);
extern int SDL_SendQuit(void);
extern void SDL_QuitQuit(void);

/* The event filter function */
extern SDL_EventFilter SDL_EventOK;
extern void *SDL_EventOKParam;

/* The array of event processing states */
extern Uint8 SDL_ProcessEvents[SDL_NUMEVENTS];

/* vi: set ts=4 sw=4 expandtab: */
