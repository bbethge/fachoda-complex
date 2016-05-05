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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "SDL.h"

SDL_Scancode getkey(void)
{
    SDL_Event event;
    while (SDL_WaitEvent(&event) >= 0) {
        switch (event.type) {
            case SDL_KEYDOWN:
                return event.key.keysym.scancode;
            case SDL_QUIT:
                exit(0);
        }
    }
    assert(!"Error in SDL_WaitEvent()");
    return -1;  // ??
}

int main(void)
{
    // Init SDL

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr,"Cannot initialize SDL: %s\n",SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    SDL_Window *window = SDL_CreateWindow("Get Key Name", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 100, 100, 0);
    if (! window) {
        fprintf(stderr,"Couldn't open a window: %s\n",SDL_GetError());
        exit(1);
    }

    printf("Focus the SDL window and press any key to see it's name (esc to quit)...\n");

    while (1) {
        SDL_Scancode k = getkey();
        printf("%s\n", SDL_GetScancodeName(k));
        if (k == SDL_SCANCODE_ESCAPE) break;
    }

    return 0;
}
