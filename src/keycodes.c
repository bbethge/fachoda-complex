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
#include <math.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdint.h>
#include "SDL_keyboard.h"
#include "SDL_scancode.h"
#include "proto.h"
#include "config.h"
#include "keycodesdef.h"

struct kc gkeys[NBKEYS] = {
    [kc_esc]            = { SDL_SCANCODE_ESCAPE, "key_quit" },
    [kc_yes]            = { SDL_SCANCODE_Y, "key_yes" },
    [kc_no]             = { SDL_SCANCODE_N, "key_no" },
    [kc_motormore]      = { SDL_SCANCODE_EQUALS, "key_throttle_more" },
    [kc_motorless]      = { SDL_SCANCODE_MINUS, "key_throttle_less" },
    [kc_externview]     = { SDL_SCANCODE_F5, "key_view_external" },
    [kc_travelview]     = { SDL_SCANCODE_F6, "key_view_still" },
    [kc_internview]     = { SDL_SCANCODE_F4, "key_view_internal" },
    [kc_nextbot]        = { SDL_SCANCODE_F2, "key_view_next" },
    [kc_prevbot]        = { SDL_SCANCODE_F3, "key_view_previous" },
    [kc_mybot]          = { SDL_SCANCODE_F1, "key_view_self" },
    [kc_mapmode]        = { SDL_SCANCODE_F9, "key_view_map" },
    [kc_zoomout]        = { SDL_SCANCODE_F7, "key_zoom_out" },
    [kc_zoomin]         = { SDL_SCANCODE_F8, "key_zoom_in" },
    [kc_riseview]       = { SDL_SCANCODE_UP, "key_look_raise" },
    [kc_lowerview]      = { SDL_SCANCODE_DOWN, "key_look_lower" },
    [kc_leftenview]     = { SDL_SCANCODE_LEFT, "key_look_left" },
    [kc_rightenview]    = { SDL_SCANCODE_RIGHT, "key_look_right" },
    [kc_towardview]     = { SDL_SCANCODE_HOME, "key_look_ahead" },
    [kc_backview]       = { SDL_SCANCODE_END, "key_look_back" },
    [kc_leftview]       = { SDL_SCANCODE_DELETE, "key_look_at_left" },
    [kc_rightview]      = { SDL_SCANCODE_PAGEDOWN, "key_look_at_right" },
    [kc_upview]         = { SDL_SCANCODE_PAGEUP, "key_look_up" },
    [kc_movetowardview] = { SDL_SCANCODE_INSERT, "key_look_panel" },
    [kc_gear]           = { SDL_SCANCODE_G, "key_gears" },
    [kc_flaps]          = { SDL_SCANCODE_F, "key_flaps" },
    [kc_brakes]         = { SDL_SCANCODE_B, "key_brakes" },
    [kc_autopilot]      = { SDL_SCANCODE_P, "key_autopilot" },
    [kc_business]       = { SDL_SCANCODE_F10, "key_buy" },
    [kc_pause]          = { SDL_SCANCODE_PAUSE, "key_pause" },
    [kc_highscores]     = { SDL_SCANCODE_TAB, "key_scores" },
    [kc_accelmode]      = { SDL_SCANCODE_X, "key_acceleration" },
    [kc_basenav]        = { SDL_SCANCODE_N, "key_navpoint_to_base" },
    [kc_suicide]        = { SDL_SCANCODE_F12, "key_suicide" },
    [kc_markpos]        = { SDL_SCANCODE_C, "key_flag_map" },
    [kc_alti]           = { SDL_SCANCODE_H, "key_cheat_up" },
    [kc_gunned]         = { SDL_SCANCODE_J, "key_cheat_gunme" },
    [kc_down]           = { SDL_SCANCODE_KP_8, "key_nose_down" },
    [kc_up]             = { SDL_SCANCODE_KP_2, "key_nose_up" },
    [kc_left]           = { SDL_SCANCODE_KP_4, "key_roll_left" },
    [kc_right]          = { SDL_SCANCODE_KP_6, "key_roll_right" },
    [kc_center]         = { SDL_SCANCODE_KP_5, "key_center_stick" },
    [kc_fire]           = { SDL_SCANCODE_SPACE, "key_shoot" },
    [kc_weapon]         = { SDL_SCANCODE_RCTRL, "key_alt_weapon" },
};

static SDL_Scancode sdl_key_of_name(char const *name)
{
    SDL_Scancode k = SDL_GetScancodeFromName(name);

    if (k == SDL_SCANCODE_UNKNOWN) {
        fprintf(stderr, "Unknown key \"%s\"\n", name);
    }
    return k;
}

void keys_load(void)
{
    for (unsigned i = 0; i < ARRAY_LEN(gkeys); i++) {
        char const *keyname = config_get_string(gkeys[i].varname, NULL);
        if (keyname) {
            SDL_Scancode kc = sdl_key_of_name(keyname);
            if (kc != SDL_SCANCODE_UNKNOWN) gkeys[i].kc = kc;
        }
    }
}

