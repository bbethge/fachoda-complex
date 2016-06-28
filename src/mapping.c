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

static struct {
    GLuint program;
    GLint position, tex_coord;
    GLint tex_scale, texture;
} map_shader;

static struct {
    GLuint program;
    GLint position, color, tex_coord;
    GLint tex_scale, texture;
} phong_shader;

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

    GLuint map_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 VaryingColor;\n"
        "in vec2 VaryingTexCoord;\n"
        "\n"
        "uniform sampler2D Texture;\n"
        "\n"
        "void main() {\n"
        "    gl_FragColor = texture(Texture, VaryingTexCoord);\n"
        "}\n"
    );
    map_shader.program = link_shader_program(
        __FILE__, __LINE__, default_vertex_shader, map_fragment_shader
    );
    map_shader.position = glGetAttribLocation(map_shader.program, "Position");
    map_shader.tex_coord = glGetAttribLocation(map_shader.program, "TexCoord");
    map_shader.tex_scale = glGetUniformLocation(map_shader.program, "TexScale");
    map_shader.texture = glGetUniformLocation(map_shader.program, "Texture");
    if (
        map_shader.position < 0 || map_shader.tex_coord < 0
        || map_shader.tex_scale < 0 || map_shader.texture < 0
    ) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "Invalid shader program "
            "(unable to access vertex attributes or uniforms)", NULL
        );
    }

    GLuint phong_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 VaryingColor;\n"
        "in vec2 VaryingTexCoord;\n"
        "\n"
        "uniform sampler2D Texture;\n"
        "\n"
        "void main() {\n"
        "    gl_FragColor = VaryingColor + texture(Texture, VaryingTexCoord);\n"
        "}\n"
    );
    phong_shader.program = link_shader_program(
        __FILE__, __LINE__, default_vertex_shader, phong_fragment_shader
    );
    phong_shader.position = glGetAttribLocation(phong_shader.program, "Position");
    phong_shader.color = glGetAttribLocation(phong_shader.program, "Color");
    phong_shader.tex_coord = glGetAttribLocation(
        phong_shader.program, "TexCoord"
    );
    phong_shader.tex_scale = glGetUniformLocation(
        phong_shader.program, "TexScale"
    );
    phong_shader.texture = glGetUniformLocation(phong_shader.program, "Texture");
    if (
        phong_shader.position < 0 || phong_shader.color < 0
        || phong_shader.tex_coord < 0 || phong_shader.tex_scale < 0
        || phong_shader.texture < 0
    ) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "Invalid shader program "
            "(unable to access vertex attributes or uniforms)", NULL
        );
    }

    mapping = malloc(256*256*sizeof(*mapping));
}

void polymap(struct vectorm *p1, struct vectorm *p2, struct vectorm *p3) {
    GLfloat v[3*3] = {
        p1->v.x, p1->v.y, p1->v.z,
        p2->v.x, p2->v.y, p2->v.z,
        p3->v.x, p3->v.y, p3->v.z
    };
    GLfloat tc[3*2] = {
        p1->mx/255.f, p1->my/255.f,
        p2->mx/255.f, p2->my/255.f,
        p3->mx/255.f, p3->my/255.f
    };
    GLuint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glUseProgram(map_shader.program);
    glVertexAttribPointer(map_shader.position, 3, GL_FLOAT, GL_FALSE, 0, v);
    glVertexAttribPointer(map_shader.tex_coord, 2, GL_FLOAT, GL_FALSE, 0, tc);
    glEnableVertexAttribArray(map_shader.position);
    glEnableVertexAttribArray(map_shader.tex_coord);
    glUniform1i(map_shader.texture, 0);
    glEnable(GL_TEXTURE_2D);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV, mapping
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform2f(map_shader.tex_scale, 1, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR
    );
    glDisable(GL_TEXTURE_2D);
    glDisableVertexAttribArray(map_shader.position);
    glDisableVertexAttribArray(map_shader.tex_coord);
    glUseProgram(prev_program);
}

void polyphong(struct vectorlum *p1, struct vectorlum *p2, struct vectorlum *p3, struct pixel coul) {
    GLfloat v[3*3] = {
        p1->v.x, p1->v.y, p1->v.z,
        p2->v.x, p2->v.y, p2->v.z,
        p3->v.x, p3->v.y, p3->v.z
    };
    GLfloat tc[3*2] = {
        p1->xl/(float)(1<<(16-vf)), p1->yl/(float)(1<<(16-vf)),
        p2->xl/(float)(1<<(16-vf)), p2->yl/(float)(1<<(16-vf)),
        p3->xl/(float)(1<<(16-vf)), p3->yl/(float)(1<<(16-vf))
    };
    GLuint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glUseProgram(phong_shader.program);
    glUniform1i(phong_shader.texture, 0);
    glBindTexture(GL_TEXTURE_2D, precatex);
    glEnable(GL_TEXTURE_2D);
    glVertexAttribPointer(phong_shader.position, 3, GL_FLOAT, GL_FALSE, 0, v);
    glVertexAttribPointer(phong_shader.tex_coord, 2, GL_FLOAT, GL_FALSE, 0, tc);
    glEnableVertexAttribArray(phong_shader.position);
    glVertexAttrib4Nub(phong_shader.color, coul.r, coul.g, coul.b, 0xFF);
    glEnableVertexAttribArray(phong_shader.tex_coord);
    glUniform2f(phong_shader.tex_scale, 1, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(phong_shader.position);
    glDisableVertexAttribArray(phong_shader.tex_coord);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(prev_program);
}
