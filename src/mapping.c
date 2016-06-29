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

static struct {
    GLuint program;
    GLint position, tex_coord;
    GLint tex_scale, texture;
} map_shader;

void initmapping(void)
{
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
