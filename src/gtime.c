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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include "gtime.h"
#include "proto.h"

//#define DT_DEBUG

static bool running;
static gtime last;
static gtime last_start_time;   // when we started running (if running)
static gtime accelerated;   // how many usecs were skipped due to acceleration since last start
static gtime leaked;    // the usecs of wall clock we skipped because we were late
static gtime prev_gtime;    // gtime_now() value last time gtime_next() was called

static gtime tv_2_gtime(struct timeval const *tv)
{
    return tv->tv_sec * 1000000 + tv->tv_usec;
}

static gtime getgtimeofday(void)
{
    struct timeval tv;
    if (0 != gettimeofday(&tv, NULL)) {
        fprintf(stderr, "Cannot gettimeofday(): %s\n", strerror(errno));
        abort();
    }
    return tv_2_gtime(&tv);
}

gtime gtime_now(void)
{
    if (! running) return last;

    last = getgtimeofday() - last_start_time + accelerated - leaked;
#   ifdef DT_DEBUG
    printf("gtime_now (running) -> %"PRIuLEAST64"\n", last);
#   endif
    return last;
}

gtime gtime_last(void)
{
    return last;
}

gtime gtime_age(gtime date)
{
    gtime now = gtime_last();
#   ifdef DT_DEBUG
    printf("age(%"PRIuLEAST64") = %"PRIuLEAST64"\n", date, now-date);
#   endif
    return now - date;
}

void gtime_stop(void)
{
#   ifdef DT_DEBUG
    printf("Stopping game time\n");
#   endif
    if (! running) return;
    (void)gtime_now();  // update 'last'
    running = false;
}

void gtime_start(void)
{
#   ifdef DT_DEBUG
    printf("Starting game time\n");
#   endif
    running = true;
    last_start_time = getgtimeofday();
    prev_gtime = gtime_now();
    accelerated = 0;
    leaked = 0;
}

void gtime_toggle(void)
{
    if (running) {
        gtime_stop();
    } else {
        gtime_start();
    }
}

void gtime_accel(gtime t)
{
    if (! running) return;
    accelerated += t;
}

gtime gtime_next(void)
{
    gtime now, dt;
    while (1) {
        now = gtime_now();
        dt = now - prev_gtime + 1;
#       ifdef DT_DEBUG
        printf("gtime_next(): dt=%"PRIuLEAST64"\n", dt);
#       endif
        if (dt > MAX_DT) {
            dt = MAX_DT;
            gtime const leak = dt - MAX_DT;
            leaked += leak;
            last = now = now - leak;
#           ifdef DT_DEBUG
            printf("gtime_next(): leak %"PRIuLEAST64"usecs\n", leak);
#           endif
        }
        if (dt >= MIN_DT) break;
#       ifdef DT_DEBUG
        printf("gtime_next(): usleep(%"PRIuLEAST64")\n", MIN_DT - dt);
#       endif
        usleep(MIN_DT - dt); // give excess CPU to the system
        if (game_paused || game_suspended) break;
    }
#   ifdef DT_DEBUG
    printf("gtime_next(): prev_gtime is now %"PRIuLEAST64"\n", now);
#   endif
    prev_gtime = now;
    return dt;
}

float gtime_next_sec(void)
{
    gtime const dt = gtime_next();
    return dt / (float)ONE_SECOND;
}

