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
#include <math.h>
#include <values.h>
#include "proto.h"
#include "video_sdl.h"
#include "SDL_opengl.h"

int *mapping;
static uint8_t preca[256];
static GLuint precatex;

#define MAX_PRECA 180
void initmapping(void)
{
    GLubyte texbuf[256*256];
    for (int a = 0; a < 256; a++) {
        preca[a] = MAX_PRECA * exp(-a/50.);
    }
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            texbuf[256*a+b] = MAX_PRECA * exp(-hypot(a,b)/50.);
        }
    }
    glGenTextures(1, &precatex);
    glBindTexture(GL_TEXTURE_2D, precatex);
    glTexImage2D(
            GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE,
            GL_UNSIGNED_BYTE, texbuf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    mapping = malloc(256*256*sizeof(*mapping));
}

void polymap(struct vect2dm *p1, struct vect2dm *p2, struct vect2dm *p3) {
    glEnable(GL_TEXTURE_2D);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV, mapping);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_TRIANGLES);
    glTexCoord2f(p1->mx/255.f, p1->my/255.f);
    glVertex2i(p1->v.x, p1->v.y);
    glTexCoord2f(p2->mx/255.f, p2->my/255.f);
    glVertex2i(p2->v.x, p2->v.y);
    glTexCoord2f(p3->mx/255.f, p3->my/255.f);
    glVertex2i(p3->v.x, p3->v.y);
    glEnd();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
}

void polyphong(struct vect2dlum *p1, struct vect2dlum *p2, struct vect2dlum *p3, struct pixel coul) {
    glBindTexture(GL_TEXTURE_2D, precatex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glEnable(GL_TEXTURE_2D);
    glColor3ub(coul.r, coul.g, coul.b);
    glBegin(GL_TRIANGLES);
    glTexCoord2d(p1->xl/(float)(1<<(16-vf)), p1->yl/(float)(1<<(16-vf)));
    glVertex2i(p1->v.x, p1->v.y);
    glTexCoord2d(p2->xl/(float)(1<<(16-vf)), p2->yl/(float)(1<<(16-vf)));
    glVertex2i(p2->v.x, p2->v.y);
    glTexCoord2d(p3->xl/(float)(1<<(16-vf)), p3->yl/(float)(1<<(16-vf)));
    glVertex2i(p3->v.x, p3->v.y);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
