/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

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

#include "SDL_assert.h"
#include "SDL.h"

#if (SDL_ASSERT_LEVEL > 0)

#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#else  /* fprintf, _exit(), etc. */
#include <stdio.h>
#include <stdlib.h>
#endif

/* We can keep all triggered assertions in a singly-linked list so we can
 *  generate a report later.
 */
#if !SDL_ASSERTION_REPORT_DISABLED
static SDL_assert_data assertion_list_terminator = { 0, 0, 0, 0, 0, 0, 0 };
static SDL_assert_data *triggered_assertions = &assertion_list_terminator;
#endif

static void 
debug_print(const char *fmt, ...)
//#ifdef __GNUC__
//__attribute__((format (printf, 1, 2)))
//#endif
{
#ifdef _WINDOWS
    /* Format into a buffer for OutputDebugStringA(). */
    char buf[1024];
    char *startptr;
    char *ptr;
    int len;
    va_list ap;
    va_start(ap, fmt);
    len = (int) SDL_vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);

    /* Visual C's vsnprintf() may not null-terminate the buffer. */
    if ((len >= sizeof (buf)) || (len < 0)) {
        buf[sizeof (buf) - 1] = '\0';
    }

    /* Write it, sorting out the Unix newlines... */
    startptr = buf;
    for (ptr = startptr; *ptr; ptr++) {
        if (*ptr == '\n') {
            *ptr = '\0';
            OutputDebugStringA(startptr);
            OutputDebugStringA("\r\n");
            startptr = ptr+1;
        }
    }

    /* catch that last piece if it didn't have a newline... */
    if (startptr != ptr) {
        OutputDebugStringA(startptr);
    }
#else
    /* Unix has it easy. Just dump it to stderr. */
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
#endif
}


#ifdef _WINDOWS
static SDL_assert_state SDL_Windows_AssertChoice = SDL_ASSERTION_ABORT;
static const SDL_assert_data *SDL_Windows_AssertData = NULL;

static LRESULT CALLBACK
SDL_Assertion_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            /* !!! FIXME: all this code stinks. */
            const SDL_assert_data *data = SDL_Windows_AssertData;
            char buf[1024];
            const int w = 100;
            const int h = 25;
            const int gap = 10;
            int x = gap;
            int y = 50;
            int len;
            int i;
            static const struct { 
                const char *name;
                SDL_assert_state state;
            } buttons[] = {
                {"Abort", SDL_ASSERTION_ABORT },
                {"Break", SDL_ASSERTION_BREAK },
                {"Retry", SDL_ASSERTION_RETRY },
                {"Ignore", SDL_ASSERTION_IGNORE },
                {"Always Ignore", SDL_ASSERTION_ALWAYS_IGNORE },
            };

            len = (int) SDL_snprintf(buf, sizeof (buf), 
                         "Assertion failure at %s (%s:%d), triggered %u time%s:\r\n  '%s'",
                         data->function, data->filename, data->linenum,
                         data->trigger_count, (data->trigger_count == 1) ? "" : "s",
                         data->condition);
            if ((len < 0) || (len >= sizeof (buf))) {
                buf[sizeof (buf) - 1] = '\0';
            }

            CreateWindowA("STATIC", buf,
                         WS_VISIBLE | WS_CHILD | SS_LEFT,
                         x, y, 550, 100,
                         hwnd, (HMENU) 1, NULL, NULL);
            y += 110;

            for (i = 0; i < (sizeof (buttons) / sizeof (buttons[0])); i++) {
                CreateWindowA("BUTTON", buttons[i].name,
                         WS_VISIBLE | WS_CHILD,
                         x, y, w, h,
                         hwnd, (HMENU) buttons[i].state, NULL, NULL);
                x += w + gap;
            }
            break;
        }

        case WM_COMMAND:
            SDL_Windows_AssertChoice = ((SDL_assert_state) (LOWORD(wParam)));
            SDL_Windows_AssertData = NULL;
            break;

        case WM_DESTROY:
            SDL_Windows_AssertData = NULL;
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static SDL_assert_state
SDL_PromptAssertion_windows(const SDL_assert_data *data)
{
    HINSTANCE hInstance = 0;  /* !!! FIXME? */
    HWND hwnd;
    MSG msg;
    WNDCLASS wc = {0};

    SDL_Windows_AssertChoice = SDL_ASSERTION_ABORT;
    SDL_Windows_AssertData = data;

    wc.lpszClassName = TEXT("SDL_assert");
    wc.hInstance = hInstance ;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpfnWndProc = SDL_Assertion_WndProc;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
  
    RegisterClass(&wc);
    hwnd = CreateWindow(wc.lpszClassName, TEXT("SDL assertion failure"),
                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                 150, 150, 570, 260, 0, 0, hInstance, 0);  

    while (GetMessage(&msg, NULL, 0, 0) && (SDL_Windows_AssertData != NULL)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, hInstance);
    return SDL_Windows_AssertChoice;
}
#endif


static void SDL_AddAssertionToReport(SDL_assert_data *data)
{
#if !SDL_ASSERTION_REPORT_DISABLED
    /* (data) is always a static struct defined with the assert macros, so
       we don't have to worry about copying or allocating them. */
    if (data->next == NULL) {  /* not yet added? */
        data->next = triggered_assertions;
        triggered_assertions = data;
    }
#endif
}

static void SDL_GenerateAssertionReport(void)
{
#if !SDL_ASSERTION_REPORT_DISABLED
    if (triggered_assertions != &assertion_list_terminator)
    {
        SDL_assert_data *item = triggered_assertions;

        debug_print("\n\nSDL assertion report.\n");
        debug_print("All SDL assertions between last init/quit:\n\n");

        while (item != &assertion_list_terminator) {
            debug_print(
                "'%s'\n"
                "    * %s (%s:%d)\n"
                "    * triggered %u time%s.\n"
                "    * always ignore: %s.\n",
                item->condition, item->function, item->filename,
                item->linenum, item->trigger_count,
                (item->trigger_count == 1) ? "" : "s",
                item->always_ignore ? "yes" : "no");
            item = item->next;
        }
        debug_print("\n");

        triggered_assertions = &assertion_list_terminator;
    }
#endif
}


static void SDL_AbortAssertion(void)
{
    SDL_Quit();
#ifdef _WINDOWS
    ExitProcess(42);
#elif unix || __APPLE__
    _exit(42);
#else
    #error Please define your platform or set SDL_ASSERT_LEVEL to 0.
#endif
}
    

static SDL_assert_state SDL_PromptAssertion(const SDL_assert_data *data)
{
    const char *envr;

    debug_print("\n\n"
                "Assertion failure at %s (%s:%d), triggered %u time%s:\n"
                "  '%s'\n"
                "\n",
                data->function, data->filename, data->linenum,
                data->trigger_count, (data->trigger_count == 1) ? "" : "s",
                data->condition);

	/* let env. variable override, so unit tests won't block in a GUI. */
    envr = SDL_getenv("SDL_ASSERT");
    if (envr != NULL) {
        if (SDL_strcmp(envr, "abort") == 0) {
            return SDL_ASSERTION_ABORT;
        } else if (SDL_strcmp(envr, "break") == 0) {
            return SDL_ASSERTION_BREAK;
        } else if (SDL_strcmp(envr, "retry") == 0) {
            return SDL_ASSERTION_RETRY;
        } else if (SDL_strcmp(envr, "ignore") == 0) {
            return SDL_ASSERTION_IGNORE;
        } else if (SDL_strcmp(envr, "always_ignore") == 0) {
            return SDL_ASSERTION_ALWAYS_IGNORE;
        } else {
            return SDL_ASSERTION_ABORT;  /* oh well. */
        }
    }

    /* platform-specific UI... */

#ifdef _WINDOWS
    return SDL_PromptAssertion_windows(data);

#elif __APPLE__
    /* This has to be done in an Objective-C (*.m) file, so we call out. */
    extern SDL_assert_state SDL_PromptAssertion_cocoa(const SDL_assert_data *);
    return SDL_PromptAssertion_cocoa(data);

#elif unix
    /* this is a little hacky. */
    for ( ; ; ) {
        char buf[32];
        fprintf(stderr, "Abort/Break/Retry/Ignore/AlwaysIgnore? [abriA] : ");
        fflush(stderr);
        if (fgets(buf, sizeof (buf), stdin) == NULL) {
            return SDL_ASSERTION_ABORT;
        }

        if (SDL_strcmp(buf, "a") == 0) {
            return SDL_ASSERTION_ABORT;
        } else if (SDL_strcmp(envr, "b") == 0) {
            return SDL_ASSERTION_BREAK;
        } else if (SDL_strcmp(envr, "r") == 0) {
            return SDL_ASSERTION_RETRY;
        } else if (SDL_strcmp(envr, "i") == 0) {
            return SDL_ASSERTION_IGNORE;
        } else if (SDL_strcmp(envr, "A") == 0) {
            return SDL_ASSERTION_ALWAYS_IGNORE;
        }
    }

#else
    #error Please define your platform or set SDL_ASSERT_LEVEL to 0.
#endif

    return SDL_ASSERTION_ABORT;
}


static SDL_mutex *assertion_mutex = NULL;

SDL_assert_state
SDL_ReportAssertion(SDL_assert_data *data, const char *func, int line)
{
    SDL_assert_state state;

    if (SDL_LockMutex(assertion_mutex) < 0) {
        return SDL_ASSERTION_IGNORE;   /* oh well, I guess. */
    }

    /* doing this because Visual C is upset over assigning in the macro. */
    if (data->trigger_count == 0) {
        data->function = func;
		data->linenum = line;
    }

    SDL_AddAssertionToReport(data);

    data->trigger_count++;
    if (data->always_ignore) {
        SDL_UnlockMutex(assertion_mutex);
        return SDL_ASSERTION_IGNORE;
    }

    state = SDL_PromptAssertion(data);

    switch (state)
    {
        case SDL_ASSERTION_ABORT:
            SDL_UnlockMutex(assertion_mutex);  /* in case we assert in quit. */
            SDL_AbortAssertion();
            return SDL_ASSERTION_IGNORE;  /* shouldn't return, but oh well. */

        case SDL_ASSERTION_ALWAYS_IGNORE:
            state = SDL_ASSERTION_IGNORE;
            data->always_ignore = 1;
            break;

        case SDL_ASSERTION_IGNORE:
        case SDL_ASSERTION_RETRY:
        case SDL_ASSERTION_BREAK:
            break;  /* macro handles these. */
    }

    SDL_UnlockMutex(assertion_mutex);

    return state;
}

#endif  /* SDL_ASSERT_LEVEL > 0 */


int SDL_AssertionsInit(void)
{
#if (SDL_ASSERT_LEVEL > 0)
    assertion_mutex = SDL_CreateMutex();
    if (assertion_mutex == NULL) {
        return -1;
    }
#endif
    return 0;
}

void SDL_AssertionsQuit(void)
{
#if (SDL_ASSERT_LEVEL > 0)
    SDL_GenerateAssertionReport();
    SDL_DestroyMutex(assertion_mutex);
    assertion_mutex = NULL;
#endif
}

/* vi: set ts=4 sw=4 expandtab: */

