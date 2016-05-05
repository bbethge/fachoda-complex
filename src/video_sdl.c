// -*- c-basic-offset: 4; c-backslash-column: 79; indent-tabs-mode: nil -*-
// vim:sw=4 ts=4 sts=4 expandtab
/* Copyright 2012
 * This file is part of Fachoda.
 *
 * Fachoda is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fachoda is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fachoda.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "proto.h"
#include "video_sdl.h"
#include "sound.h"
#include "SDL_opengl.h"

static SDL_Window *window;

void initvideo(bool fullscreen)
{
    int w, h;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr,"Couln't initialize SDL: %s\n",SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    window = SDL_CreateWindow("FACHODA Complex", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, win_width, win_height, SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|(fullscreen?SDL_WINDOW_FULLSCREEN:0));
    if (! window) {
        fprintf(stderr,"Couldn't open a window: %s\n",SDL_GetError());
        exit(1);
    }
    SDL_GetWindowSize(window, &w, &h);
    printf("Set %dx%d mode\n",w,h);
    if (! SDL_GL_CreateContext(window)) {
        fprintf(stderr,"Couldn't create an OpenGL context: %s\n",SDL_GetError());
        exit(1);
    }
    glViewport(0, 0, win_width, win_height);
    glPixelZoom(1, -1);
    glFrontFace(GL_CW);
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, win_width, win_height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    SDL_ShowCursor(0);
    SDL_EventState(SDL_WINDOWEVENT, SDL_ENABLE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
}

void buffer2video(void)
{
    SDL_GL_SwapWindow(window);
}

int xmouse, ymouse;
static bool mouse_button[7];

/* We use a bitfields of SDL_NUM_SCANCODES pairs of bits.
 * For each key, the first bit gives the actual pressed status,
 * and the second bit is set whenever the key was pressed in the past,
 * and is only reset to 0 when the key is processed (by kreset). */
static uint8_t keytab[(SDL_NUM_SCANCODES+3)/4];

static unsigned bit_of_key(SDL_Scancode k)
{
    assert(k < SDL_NUM_SCANCODES);
    return (unsigned) k;
}

// Set both keybits to 1
static void bitset(unsigned n)
{
    keytab[n/4] |= 3U<<((n&3)*2);
}

// Reset both keybits to 0
static void bitzero(unsigned n)
{
    keytab[n/4] &= ~(3U<<((n&3)*2));
}

// Reset the current keybit to 0 (but keep the past keybit)
static void bitzero1(unsigned n)
{
    keytab[n/4] &= ~(1U<<((n&3)*2));
}

// Test the current keybit
static bool bittest1(unsigned n)
{
    return !!(keytab[n/4] & (1U<<((n&3)*2)));
}

// Test any of the keybits
static bool bittest(unsigned n)
{
    return !!(keytab[n/4] & (3U<<((n&3)*2)));
}

bool kread(SDL_Scancode k)
{
    return bittest1(bit_of_key(k));
}

bool kreset(SDL_Scancode k)
{
    unsigned const n = bit_of_key(k);
    bool const r = bittest(n);
    if (r) bitzero(n);
    return r;
}

bool button_read(unsigned b)
{
    assert(b < ARRAY_LEN(mouse_button));
    return mouse_button[b];
}

bool button_reset(unsigned b)
{
    assert(b < ARRAY_LEN(mouse_button));
    bool r = mouse_button[b];
    mouse_button[b] = false;
    return r;
}

void xproceed(void)
{
    SDL_Event event;

    // Pointer position
    SDL_GetMouseState(&xmouse, &ymouse);
    if (xmouse < 0) xmouse = 0;
    if (ymouse < 0) ymouse = 0;
    if (xmouse >= win_width) xmouse = win_width-1;
    if (ymouse >= win_height) ymouse = win_height-1;

    // Keys
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                bitset(bit_of_key(event.key.keysym.scancode));
                break;
            case SDL_KEYUP:
                bitzero1(bit_of_key(event.key.keysym.scancode));
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button < ARRAY_LEN(mouse_button)) {
                    mouse_button[event.button.button] = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button < ARRAY_LEN(mouse_button)) {
                    mouse_button[event.button.button] = false;
                }
                break;
            case SDL_QUIT:
                quit_game = true;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_MINIMIZED:
                        game_suspended = 1;
                        sound_suspend();
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                        game_suspended = 0;
                        sound_resume();
                        break;
                }
                break;
        }
    }
}

