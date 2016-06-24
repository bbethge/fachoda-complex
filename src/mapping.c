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
#define GL_GLEXT_PROTOTYPES
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

void polymap(struct vectorm *p1, struct vectorm *p2, struct vectorm *p3) {
    GLuint program;
    GLint position, tex_coord;
    GLfloat v[3][3] = {
        { p1->v.x, p1->v.y, p1->v.z },
        { p2->v.x, p2->v.y, p2->v.z },
        { p3->v.x, p3->v.y, p3->v.z }
    };
    GLfloat tc[3][2] = {
        { p1->mx/255.f, p1->my/255.f },
        { p2->mx/255.f, p2->my/255.f },
        { p3->mx/255.f, p3->my/255.f }
    };
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    position = glGetAttribLocation(program, "position");
    tex_coord = glGetAttribLocation(program, "tex_coord");
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, (void*)v[1] - (void*)v[0], v);
    glVertexAttribPointer(tex_coord, 2, GL_FLOAT, GL_FALSE, (void*)tc[1] - (void*)tc[0], tc);
    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(tex_coord);
    glEnable(GL_TEXTURE_2D);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV, mapping);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glDisableVertexAttribArray(position);
    glDisableVertexAttribArray(tex_coord);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribPointer(tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void polyphong(struct vectorlum *p1, struct vectorlum *p2, struct vectorlum *p3, struct pixel coul) {
    GLuint program;
    GLint position, color, tex_coord;
    GLfloat v[3][3] = {
        { p1->v.x, p1->v.y, p1->v.z },
        { p2->v.x, p2->v.y, p2->v.z },
        { p3->v.x, p3->v.y, p3->v.z }
    };
    GLfloat tc[3][2] = {
        { p1->xl/(float)(1<<(16-vf)), p1->yl/(float)(1<<(16-vf)) },
        { p2->xl/(float)(1<<(16-vf)), p2->yl/(float)(1<<(16-vf)) },
        { p3->xl/(float)(1<<(16-vf)), p3->yl/(float)(1<<(16-vf)) }
    };
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    position = glGetAttribLocation(program, "position");
    color = glGetAttribLocation(program, "color");
    tex_coord = glGetAttribLocation(program, "tex_coord");
    glBindTexture(GL_TEXTURE_2D, precatex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glEnable(GL_TEXTURE_2D);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, (void*)v[1] - (void*)v[0], v);
    glVertexAttribPointer(tex_coord, 2, GL_FLOAT, GL_FALSE, (void*)tc[1] - (void*)tc[0], tc);
    glEnableVertexAttribArray(position);
    glVertexAttrib4Nub(color, coul.r, coul.g, coul.b, 0xFF);
    glEnableVertexAttribArray(tex_coord);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(position);
    glDisableVertexAttribArray(tex_coord);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribPointer(tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
