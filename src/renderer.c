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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <values.h>
#include <assert.h>
#define GL_GLEXT_PROTOTYPES
#include "video_sdl.h"
#include "heightfield.h"
#include "SDL_opengl.h"

#define STRING(x) #x
#define XSTRING(x) STRING(x)

struct {
    GLuint program;
    GLint position, color, size;
} phare_shader, blob_shader;

static struct {
    GLuint program;
    GLint position, normal, color;
    GLint light_matrix, preca;
} phong_shader;

#define MAX_PRECA 180
#define PRECA_SIZE 256
static GLuint precatex;

struct vectorlum *pts;
struct matrix *oL;

void initrender() {
    pts = malloc(3000*sizeof(*pts));   // nbpts max par objets
#define MAXNO 5000
    oL = malloc(MAXNO*sizeof(*oL));   // nb objet max dans un ak

    GLuint sphere_vertex_shader = compile_shader(
        GL_VERTEX_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "attribute vec4 Position;\n"
        "attribute vec4 Color;\n"
        "attribute float Size;\n"
        "\n"
        "out float gl_ClipDistance[2];\n"
        "out vec4 VaryingColor;\n"
        "\n"
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * Position;\n"
        "    VaryingColor = Color;\n"
        "    gl_PointSize = Size;\n"
        "    // No horizontal clipping since the center of the blob may be\n"
        "    // offscreen\n"
        "    gl_ClipDistance[0] = gl_Position.w - gl_Position.z;\n"
        "    gl_ClipDistance[1] = gl_Position.w + gl_Position.z;\n"
        "}\n"
    );
    GLuint phare_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 VaryingColor;\n"
        "\n"
        "void main() {\n"
        "    vec2 PointCoord = 2*gl_PointCoord - 1;\n"
        "    float Dist2 = dot(PointCoord, PointCoord);\n"
        "    if (Dist2 < 1) {\n"
        "        gl_FragColor.rgb =\n"
        "            VaryingColor.rgb\n"
        "            * max(sqrt(1 - Dist2), abs(PointCoord.x));\n"
        "        gl_FragColor.a = VaryingColor.a;\n"
        "    }\n"
        "    else {\n"
        "        discard;\n"
        "    }\n"
        "}\n"
    );
    phare_shader.program = link_shader_program(
        __FILE__, __LINE__, sphere_vertex_shader, phare_fragment_shader
    );
    phare_shader.position = glGetAttribLocation(
        phare_shader.program, "Position"
    );
    phare_shader.color = glGetAttribLocation(phare_shader.program, "Color");
    phare_shader.size = glGetAttribLocation(phare_shader.program, "Size");
    if (
        phare_shader.position < 0 || phare_shader.color < 0
        || phare_shader.size < 0
    ) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "Invalid shader program "
            "(unable to access vertex attributes or uniforms)", NULL
        );
    }

    GLuint blob_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 VaryingColor;\n"
        "\n"
        "void main() {\n"
        "    vec2 PointCoord = 2*gl_PointCoord - 1;\n"
        "    float Dist2 = dot(PointCoord, PointCoord);\n"
        "    if (Dist2 < 1) {\n"
        "        gl_FragColor.rgb = VaryingColor.rgb * sqrt(1 - Dist2);\n"
        "        gl_FragColor.a = VaryingColor.a;\n"
        "    }\n"
        "    else {\n"
        "        discard;\n"
        "    }\n"
        "}\n"
    );
    blob_shader.program = link_shader_program(
        __FILE__, __LINE__, sphere_vertex_shader, blob_fragment_shader
    );
    blob_shader.position = glGetAttribLocation(
        blob_shader.program, "Position"
    );
    blob_shader.color = glGetAttribLocation(blob_shader.program, "Color");
    blob_shader.size = glGetAttribLocation(blob_shader.program, "Size");
    if (
        blob_shader.position < 0 || blob_shader.color < 0
        || blob_shader.size < 0
    ) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "Invalid shader program "
            "(unable to access vertex attributes or uniforms)", NULL
        );
    }

    GLubyte preca[PRECA_SIZE];
    for (int a = 0; a < PRECA_SIZE; a++) {
        preca[a] = MAX_PRECA * exp(-a/50.);
    }
    glGenTextures(1, &precatex);
    glBindTexture(GL_TEXTURE_1D, precatex);
    glTexImage1D(
            GL_TEXTURE_1D, 0, GL_RED, PRECA_SIZE, 0, GL_RED,
            GL_UNSIGNED_BYTE, preca);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_1D, 0);

    GLuint phong_vertex_shader = compile_shader(
        GL_VERTEX_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 Position;\n"
        "in vec3 Normal;\n"
        "in vec4 Color;\n"
        "\n"
        "out float gl_ClipDistance[6];\n"
        "out vec4 VaryingColor;\n"
        "out vec3 LightVector;\n"
        "\n"
        "uniform mat3 LightMatrix;\n"
        "\n"
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * Position;\n"
        "    VaryingColor = Color;\n"
        "    // on calcule aussi les projs des\n"
        "    // norms dans le plan lumineux infiniment �loign�\n"
        "    LightVector = LightMatrix * Normal / (1<<"XSTRING(vf)");\n"
        "    gl_ClipDistance[0] = gl_Position.w - gl_Position.x;\n"
        "    gl_ClipDistance[1] = gl_Position.w + gl_Position.x;\n"
        "    gl_ClipDistance[2] = gl_Position.w - gl_Position.y;\n"
        "    gl_ClipDistance[3] = gl_Position.w + gl_Position.y;\n"
        "    gl_ClipDistance[4] = gl_Position.w - gl_Position.z;\n"
        "    gl_ClipDistance[5] = gl_Position.w + gl_Position.z;\n"
        "}\n"
    );
    GLuint phong_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER, __FILE__, __LINE__,
        "#version 130\n"
        "\n"
        "in vec4 VaryingColor;\n"
        "in vec3 LightVector;\n"
        "\n"
        "uniform sampler1D Preca;\n"
        "\n"
        "void main() {\n"
        "    gl_FragColor = VaryingColor;\n"
        "    if (LightVector.z < 0) {\n"
        "        gl_FragColor.rgb +=\n"
        "            texture(Preca, length(LightVector.xy)).r;\n"
        "    }\n"
        "}\n"
    );
    phong_shader.program = link_shader_program(
        __FILE__, __LINE__, phong_vertex_shader, phong_fragment_shader
    );
    phong_shader.position = glGetAttribLocation(
        phong_shader.program, "Position"
    );
    phong_shader.normal = glGetAttribLocation(phong_shader.program, "Normal");
    phong_shader.color = glGetAttribLocation(phong_shader.program, "Color");
    phong_shader.light_matrix = glGetUniformLocation(
        phong_shader.program, "LightMatrix"
    );
    phong_shader.preca = glGetUniformLocation(phong_shader.program, "Preca");
    if (
        phong_shader.position < 0 || phong_shader.normal < 0
        || phong_shader.color < 0 || phong_shader.light_matrix < 0
        || phong_shader.preca < 0
    ) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Error", "Invalid shader program "
            "(unable to access vertex attributes or uniforms)", NULL
        );
    }
}

void plot(int x, int y, int r)
{
    GLint v[2] = { x+win_center_x, y+win_center_y };
    glVertexAttribPointer(default_shader.position, 2, GL_INT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(default_shader.position);
    glVertexAttrib4Nub(
        default_shader.color, (r>>16)&0xFF, (r>>8)&0xFF, r&0xFF, 0xFF
    );
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableVertexAttribArray(default_shader.position);
}

// Draw a pixel at 50% opacity
void mixplot(int x, int y, int r, int g, int b){
    GLint v[2] = { x+win_center_x, y+win_center_y };
    glVertexAttribPointer(default_shader.position, 2, GL_INT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(default_shader.position);
    glVertexAttrib4Nub(default_shader.color, r, g, b, 0x7F);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableVertexAttribArray(default_shader.position);
}
// Draw a small white crosshair
// FIXME: does not reproduce original behavior
void plot_stick(int x,int y){
    GLfloat v[8*2] = {
        x-5, y,  x, y,   x+5, y,  x, y,
        x, y-5,  x, y,   x, y+5,  x, y,
    };
    GLubyte c[8*3] = {
        0xA0, 0xA0, 0xA0,  0xF0, 0xF0, 0xF0,
        0xA0, 0xA0, 0xA0,  0xF0, 0xF0, 0xF0,
        0xA0, 0xA0, 0xA0,  0xF0, 0xF0, 0xF0,
        0xA0, 0xA0, 0xA0,  0xF0, 0xF0, 0xF0,
    };
    glVertexAttribPointer(default_shader.position, 2, GL_FLOAT, GL_FALSE, 0, v);
    glVertexAttribPointer(
        default_shader.color, 3, GL_UNSIGNED_BYTE, GL_TRUE, 0, c
    );
    glEnableVertexAttribArray(default_shader.position);
    glEnableVertexAttribArray(default_shader.color);
    glPushMatrix();
    glTranslatef(win_center_x, win_center_y, 1);
    glDrawArrays(GL_LINES, 0, 8);
    glPopMatrix();
    glDisableVertexAttribArray(default_shader.position);
    glDisableVertexAttribArray(default_shader.color);
}
// Draw a tiny yellow ball
void plotboule(int x,int y) {
    plot(x,y-1,0xA0A020);
    plot(x+1,y-1,0x909020);
    plot(x-1,y,0xA0A020);
    plot(x,y,0xF0F020);
    plot(x+1,y,0xB0B020);
    plot(x+2,y,0x808020);
    plot(x-1,y+1,0x909020);
    plot(x,y+1,0xB0B020);
    plot(x+1,y+1,0xA0A020);
    plot(x+2,y+1,0x909020);
    plot(x,y+2,0x808020);
    plot(x+1,y+2,0x606020);
}
// Draw an animated cursor made of four yellow balls
void plot_cursor(int x,int y) {
    static float a=0, ar=0;
    float r=8*sin(ar);
    float c=r*cos(a);
    float s=r*sin(a);
    plotboule(x+c-win_center_x,y+s-win_center_y);
    plotboule(x+s-win_center_x,y-c-win_center_y);
    plotboule(x-c-win_center_x,y-s-win_center_y);
    plotboule(x-s-win_center_x,y+c-win_center_y);
    a+=.31; ar+=.2;
}
// Draw a circle (unfilled)
// FIXME: antialiasing does not work
#define CERCLE_MAX_SIDES 180
void cercle(int x, int y, int radius, int c) {
    int i, n = MIN(4+radius/2, CERCLE_MAX_SIDES);
    GLfloat v[CERCLE_MAX_SIDES][2];
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glVertexAttribPointer(default_shader.position, 2, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(default_shader.position);
    glVertexAttrib4Nub(
        default_shader.color, (c>>16)&0xFF, (c>>8)&0xFF, c&0xFF, 0xFF
    );
    for (i=0; i<n; i++) {
        v[i][0] = x+win_center_x+radius*cos(2*M_PI*i/n);
        v[i][1] = y+win_center_y+radius*sin(2*M_PI*i/n);
    }
    glDrawArrays(GL_LINE_LOOP, 0, n);
    glDisableVertexAttribArray(default_shader.position);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

extern inline int color_of_pixel(struct pixel c);
extern inline struct pixel32 pixel32_of_pixel(struct pixel c);

void polyflat(struct vector *p1, struct vector *p2, struct vector *p3, struct pixel coul) {
    GLfloat v[9] = {
        p1->x, p1->y, p1->z,
        p2->x, p2->y, p2->z,
        p3->x, p3->y, p3->z
    };
    glVertexAttribPointer(default_shader.position, 3, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(default_shader.position);
    glVertexAttrib4Nub(default_shader.color, coul.r, coul.g, coul.b, 0xFF);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(default_shader.position);
}
void drawline(struct vect2d const *p1, struct vect2d const *p2, int col) {
    GLint v[4] = {
        p1->x, p1->y,
        p2->x, p2->y
    };
    glVertexAttribPointer(default_shader.position, 2, GL_INT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(default_shader.position);
    glVertexAttrib4Nub(
        default_shader.color, (col>>16)&0xFF, (col>>8)&0xFF, col&0xFF, 0xFF
    );
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(default_shader.position);
}

void draw_rectangle(struct vect2d const *restrict min, struct vect2d const *restrict max, int col)
{
    struct vect2d const p1 = { .x = min->x, .y = max->y };
    struct vect2d const p2 = { .x = max->x, .y = min->y };
    drawline(min, &p1, col);
    drawline(min, &p2, col);
    drawline(max, &p1, col);
    drawline(max, &p2, col);
}

void fill_rect(GLint attrib1, ...)
{
    va_list ap;
    GLint attrib = attrib1;
    va_start(ap, attrib1);
    int nb_attrib = 0;
    while (attrib >= 0) {
        va_arg(ap, int); va_arg(ap, int); va_arg(ap, int); va_arg(ap, int);
        nb_attrib++;
        attrib = va_arg(ap, GLint);
    }
    va_end(ap);

    GLint *verts = malloc(2*4*nb_attrib*sizeof(GLint));
    attrib = attrib1;
    va_start(ap, attrib1);
    for (int i = 0; i < nb_attrib; i++) {
        int minx = va_arg(ap, int);
        int miny = va_arg(ap, int);
        int maxx = va_arg(ap, int);
        int maxy = va_arg(ap, int);
        memcpy(
            verts + 2*4*i,
            (GLint[]) { minx, miny, maxx, miny, minx, maxy, maxx, maxy },
            2*4*sizeof(GLint)
        );
        glVertexAttribPointer(attrib, 2, GL_INT, GL_FALSE, 0, verts + 2*4*i);
        glEnableVertexAttribArray(attrib);
        attrib = va_arg(ap, GLint);
    }
    va_end(ap);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    attrib = attrib1;
    va_start(ap, attrib1);
    for (int i = 0; i < nb_attrib; i++) {
        va_arg(ap, int); va_arg(ap, int); va_arg(ap, int); va_arg(ap, int);
        glVertexAttribPointer(attrib, 2, GL_INT, GL_FALSE, 0, NULL);
        glDisableVertexAttribArray(attrib);
        attrib = va_arg(ap, GLint);
    }
    va_end(ap);
    free(verts);
}

void drawline2(struct vect2d *p1, struct vect2d *p2, int col) {
    int s, x,y,xi, dy;
    struct vect2d *tmp;
    int q;
    if (p1->x>p2->x) { tmp=p1; p1=p2; p2=tmp; }
    if ((dy=(p2->y-p1->y))>0) {
        s=1;
        q = ((p2->x-p1->x)<<vf)/(1+dy);
    } else {
        dy=-dy;
        s=-1;
        q = ((p2->x-p1->x)<<vf)/(1+dy);
    }
    x = (p1->x)<<vf;
    for (y=p1->y; dy>=0; dy--, y+=s) {
        for (xi=x>>vf; xi<1+((x+q)>>vf); xi++) {
            mapping[xi+MAP_MARGIN+((y+MAP_MARGIN)<<8)]=col+0x0F0F0F;
            mapping[xi+1+MAP_MARGIN+((y+MAP_MARGIN)<<8)]=col;
            mapping[xi+1+MAP_MARGIN+((y+1+MAP_MARGIN)<<8)]=col;
            mapping[xi+MAP_MARGIN+((y+1+MAP_MARGIN)<<8)]=col+0x0F0F0F;
        }
        x+=q;
    }
}
void drawlinetb(struct vect2d *p1, struct vect2d *p2, int col) {
    int s, x,y,xi, dy;
    struct vect2d *tmp;
    int q;
    if (p1->x>p2->x) { tmp=p1; p1=p2; p2=tmp; }
    if ((dy=(p2->y-p1->y))>0) {
        s=1;
        q = ((p2->x-p1->x)<<vf)/(1+dy);
    } else {
        dy=-dy;
        s=-1;
        q = ((p2->x-p1->x)<<vf)/(1+dy);
    }
    x = (p1->x)<<vf;
    for (y=p1->y; dy>=0; dy--, y+=s) {
        for (xi=x>>vf; xi<1+((x+q)>>vf); xi++) *((int*)tbback+xi+MAP_MARGIN+(y+MAP_MARGIN)*pannel_height)=col;
        x+=q;
    }
}
void calcposrigide(int o) {
    mulmv(&obj[obj[o].objref].rot,&mod[obj[o].model].offset,&obj[o].pos);
    addv(&obj[o].pos,&obj[obj[o].objref].pos);
    copym(&obj[o].rot,&obj[obj[o].objref].rot);
    obj_check_pos(o);
}
void calcposarti(int o, struct matrix *m) {
    mulmv(&obj[obj[o].objref].rot,&mod[obj[o].model].offset,&obj[o].pos);
    addv(&obj[o].pos,&obj[obj[o].objref].pos);
    mulm3(&obj[o].rot,&obj[obj[o].objref].rot,m);
    obj_check_pos(o);
}
void calcposaind(int i) {
    int xk,yk,ak;
    xk=(int)floor(obj[i].pos.x/TILE_LEN)+(MAP_LEN>>1);
    yk=(int)floor(obj[i].pos.y/TILE_LEN)+(MAP_LEN>>1);
    if (xk<0 || xk>=MAP_LEN || yk<0 || yk>=MAP_LEN) {
        printf("HS!\n"); exit(-1);}
    ak=xk+(yk<<LOG_MAP_LEN);
    if (ak!=obj[i].ak) {
        if (obj[i].next!=-1) obj[obj[i].next].prec=obj[i].prec;
        if (obj[i].prec!=-1) obj[obj[i].prec].next=obj[i].next;
        else map[obj[i].ak].first_obj=obj[i].next;
        obj[i].next=map[ak].first_obj;
        if (map[ak].first_obj!=-1) obj[map[ak].first_obj].prec=i;
        obj[i].prec=-1;
        map[ak].first_obj=i;
        obj[i].ak=ak;
    }
}
void calcposa(void)
{
    // calcule les pos et les rot absolues des objets du monde r�el
    int xk, yk, ak;
    for (int i=0; i<nb_obj; i++) {
        if (!mod[obj[i].model].fix || !mod[obj[i].model].anchored) {
            xk=(int)floor(obj[i].pos.x/TILE_LEN)+(MAP_LEN>>1);
            yk=(int)floor(obj[i].pos.y/TILE_LEN)+(MAP_LEN>>1);
            assert(xk>=0 && xk<MAP_LEN && yk>=0 && yk<MAP_LEN);
            ak=xk+(yk<<LOG_MAP_LEN);
            if (ak!=obj[i].ak) {
                if (obj[i].next!=-1) obj[obj[i].next].prec=obj[i].prec;
                if (obj[i].prec!=-1) obj[obj[i].prec].next=obj[i].next;
                else map[obj[i].ak].first_obj=obj[i].next;
                obj[i].next=map[ak].first_obj;
                if (map[ak].first_obj!=-1) obj[map[ak].first_obj].prec=i;
                obj[i].prec=-1;
                map[ak].first_obj=i;
                obj[i].ak=ak;
            }
        }
    }
}

// Add yellow with amplitude 190*max(sqrt(r**2-(xx-x)**2-(yy-y)**2), |xx-x|)/r
// to the videobuffer within the disk (xx-x)**2+(yy-y)**2 < r**2.
void plotphare(int x, int y, int r) {
    GLuint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glUseProgram(phare_shader.program);
    for (int i = 2; i < 6; i++) {
        glDisable(GL_CLIP_DISTANCE0 + i);
    }
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glVertexAttribPointer(
        phare_shader.position, 2, GL_FLOAT, GL_FALSE, 0,
        (GLfloat [2]) { x, y }
    );
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnableVertexAttribArray(phare_shader.position);
    glVertexAttrib4Nub(phare_shader.color, 190, 190, 0, 0xFF);
    glVertexAttrib1f(phare_shader.size, 2*r);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableVertexAttribArray(phare_shader.position);
    glBlendFunc(GL_ONE, GL_ONE);
    glDisable(GL_BLEND);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    for (int i = 2; i < 6; i++) {
        glEnable(GL_CLIP_DISTANCE0 + i);
    }
    glUseProgram(prev_program);
}

// Draw grey with value inten*sqrt(r**2-(xx-x)**2-(yy-y)**2)
// within the disk (xx-x)**2+(yy-y)**2 < r**2.
static void plotblob(int x, int y, int r, int inten) {
    GLuint prev_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glUseProgram(blob_shader.program);
    for (int i = 2; i < 6; i++) {
        glDisable(GL_CLIP_DISTANCE0 + i);
    }
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glVertexAttribPointer(
        blob_shader.position, 2, GL_FLOAT, GL_FALSE, 0,
        (GLfloat [2]) { x, y }
    );
    glEnableVertexAttribArray(blob_shader.position);
    glVertexAttrib4Nub(blob_shader.color, inten, inten, inten, 0xFF);
    glVertexAttrib1f(blob_shader.size, 2*r);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisableVertexAttribArray(blob_shader.position);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    for (int i = 2; i < 6; i++) {
        glEnable(GL_CLIP_DISTANCE0 + i);
    }
    glUseProgram(prev_program);
}

// Lighten the pixels in the videobuffer within the disk
// (xx-x)**2+(yy-y)**2 < r**2 by
// 90*sqrt(r**2-(xx-x)**2-(yy-y)**2)/r.
void plotnuage(int x, int y, int r) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    plotblob(x, y, r, 90);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
}

// Darken the pixels in the videobuffer within the disk
// (xx-x)**2+(yy-y)**2 < r**2 by
// 40*sqrt(r**2-(xx-x)**2-(yy-y)**2)/r.
void plotfumee(int x, int y, int r) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
    glBlendFunc(GL_ONE, GL_ONE);
    plotblob(x, y, r, 40);
    glBlendFunc(GL_ONE, GL_ZERO);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
}

static void darken(uint8_t *b)
{
    *b = *b - ((*b>>2) & 0x3F);
}

static bool is_on_screen(struct vect2d e, double disp_radius)
{
    return
        e.x >= -disp_radius &&
        e.x <= win_width + disp_radius &&
        e.y >= -disp_radius &&
        e.y <= win_height + disp_radius;
}

static void render_sphere(int o)
{
    if (obj[o].posc.z <= 0) return;
    if (obj[o].type == TYPE_LIGHT && !night_mode) return;

    struct vect2d e;
    proj(&e, &obj[o].posc);
    double disp_radius = proj1(mod[obj[o].model].rayon, obj[o].posc.z);

    if (! is_on_screen(e, disp_radius)) return;

    if (obj[o].type == TYPE_CLOUD) {
        if (night_mode) {
            plotfumee(e.x, e.y, disp_radius);
        } else {
            plotnuage(e.x, e.y, disp_radius);
        }
    } else if (obj[o].type == TYPE_SMOKE) {
        if (smoke_radius[o - smoke_start] > 0.) {
            plotfumee(e.x, e.y, (int)(disp_radius*smoke_radius[o-smoke_start]) >> 9);
        }
    } else {
        assert(obj[o].type == TYPE_LIGHT);
        plotphare(e.x, e.y, disp_radius*4 + 1);
    }
}

static void render_obj(int o, int no)
{
    double disp_radius;

    if (obj[o].posc.z > 0) {
        struct vect2d e;
        proj(&e,&obj[o].posc);
        disp_radius = proj1(mod[obj[o].model].rayon, obj[o].posc.z);
        if (! is_on_screen(e, disp_radius)) return;
    } else {
        // Maybe some part of the object are in front nonetheless?
        double r;
        r = mod[obj[o].model].rayon*sqrt(z_near*z_near+win_center_x*win_center_x)/win_center_x;
        if (obj[o].posc.z < z_near*fabs(obj[o].posc.x)/win_center_x - r) return;
        r = mod[obj[o].model].rayon*sqrt(z_near*z_near+win_center_y*win_center_y)/win_center_y;
        if (obj[o].posc.z < z_near*fabs(obj[o].posc.y)/win_center_y - r) return;
        disp_radius = win_width;
    }

    // TODO: instead of this, alter background pixel color by disp_radius until it's >1
    if (disp_radius < .3) {
        return;
    } else if (disp_radius < .5) {
        // Draw a single dot (black?) so that we can see planes from a distance.
        if (obj[o].posc.z > 0) {
            struct vect2d e;
            proj(&e, &obj[o].posc);
            plot(e.x - win_center_x, e.y - win_center_y, 0x0);
        }
        return;
    }

    // actual drawing of meshed objects
    struct matrix co;
    int mo = obj[o].distance<(TILE_LEN*TILE_LEN*.14) ? 0 : 1;
    // on calcule aussi la position de tous les points de l'objet dans le repere de la camera, ie CamT*Obj*u
    mulmt3(&co,&obj[0].rot,&obj[o].rot);
    for (int p=0; p<mod[obj[o].model].nbpts[mo]; p++) {
        pts[p].v = mod[obj[o].model].pts[mo][p];
    }
    glPushMatrix();
    glMultTransposeMatrixf(
        (GLfloat [16]) {
            co.x.x, co.y.x, co.z.x, obj[o].posc.x,
            co.x.y, co.y.y, co.z.y, obj[o].posc.y,
            co.x.z, co.y.z, co.z.z, obj[o].posc.z,
            0,      0,      0,      1
        }
    );
    if (obj[o].type==TYPE_SHOT) {
        struct vector *pt = mod[obj[o].model].pts[mo];
        GLfloat v[6] = {
            pt[0].x, pt[0].y, pt[0].z,
            pt[1].x, pt[1].y, pt[1].z
        };
        glVertexAttribPointer(
            default_shader.position, 3, GL_FLOAT, GL_FALSE, 0, v
        );
        glEnableVertexAttribArray(default_shader.position);
        glVertexAttrib4Nub(default_shader.color, 0xFF, 0xA0, 0xF0, 0xFF);
        glDrawArrays(GL_LINES, 0, 2);
        glDisableVertexAttribArray(default_shader.position);
    } else {
        for (int p=0; p<mod[obj[o].model].nbfaces[mo]; p++) {
            if (mod[obj[o].model].fac[mo][p].norm.x != 0 ||
                    mod[obj[o].model].fac[mo][p].norm.y != 0 ||
                    mod[obj[o].model].fac[mo][p].norm.z != 0) {
                glEnable(GL_CULL_FACE);
            }
            if (obj[o].type==TYPE_INSTRUMENTS && p>=mod[obj[o].model].nbfaces[mo]-2) {
                struct vectorm pt[3];
                int i;
                for (i=0; i<3; i++) {
                    pt[i].v=mod[obj[o].model].pts[mo][mod[obj[o].model].fac[mo][p].p[i]];
                }
                if (p-(mod[obj[o].model].nbfaces[mo]-2)) {
                    pt[2].mx=MAP_MARGIN;
                    pt[2].my=pannel_width+MAP_MARGIN;
                    pt[0].mx=pannel_height+MAP_MARGIN;
                    pt[0].my=MAP_MARGIN;
                    pt[1].mx=pannel_height+MAP_MARGIN;
                    pt[1].my=pannel_width+MAP_MARGIN;
                } else {
                    pt[0].mx=MAP_MARGIN;
                    pt[0].my=pannel_width+MAP_MARGIN;
                    pt[1].mx=MAP_MARGIN;
                    pt[1].my=MAP_MARGIN;
                    pt[2].mx=pannel_height+MAP_MARGIN;
                    pt[2].my=MAP_MARGIN;
                }
                polymap(&pt[0],&pt[1],&pt[2]);
            } else {
                struct pixel coul = mod[obj[o].model].fac[mo][p].color;
                if (night_mode) {
                    if (obj[o].type != TYPE_INSTRUMENTS) {
                        darken(&coul.r);
                    }
                    darken(&coul.g);
                    darken(&coul.b);
                }
                GLuint prev_program;
                glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
                glUseProgram(phong_shader.program);
                glBindTexture(GL_TEXTURE_1D, precatex);
                glEnable(GL_TEXTURE_1D);
                glUniform1i(phong_shader.preca, 0);
                struct vector *pt[3];
                struct vector *n[3];
                for (int i = 0; i < 3; i++) {
                    pt[i] = &mod[obj[o].model].pts[mo][
                        mod[obj[o].model].fac[mo][p].p[i]
                    ];
                    n[i] = &mod[obj[o].model].norm[mo][
                        mod[obj[o].model].fac[mo][p].p[i]
                    ];
                }
                glVertexAttribPointer(
                    phong_shader.position, 3, GL_FLOAT, GL_FALSE, 0,
                    (GLfloat [3*3]) {
                        pt[0]->x, pt[0]->y, pt[0]->z,
                        pt[1]->x, pt[1]->y, pt[1]->z,
                        pt[2]->x, pt[2]->y, pt[2]->z,
                    }
                );
                glVertexAttribPointer(
                    phong_shader.normal, 3, GL_FLOAT, GL_FALSE, 0,
                    (GLfloat [3*3]) {
                        n[0]->x, n[0]->y, n[0]->z,
                        n[1]->x, n[1]->y, n[1]->z,
                        n[2]->x, n[2]->y, n[2]->z,
                    }
                );
                glUniformMatrix3fv(
                    phong_shader.light_matrix, 1, GL_FALSE, (GLfloat [3*3]) {
                        oL[no].x.x, oL[no].y.x, oL[no].z.x,  // Transposed!
                        oL[no].x.y, oL[no].y.y, oL[no].z.y,
                        oL[no].x.z, oL[no].y.z, oL[no].z.z,
                    }
                );
                glEnableVertexAttribArray(phong_shader.position);
                glEnableVertexAttribArray(phong_shader.normal);
                glVertexAttrib4Nub(
                    phong_shader.color, coul.r, coul.g, coul.b, 0xFF
                );
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glDisableVertexAttribArray(phong_shader.position);
                glDisableVertexAttribArray(phong_shader.normal);
                glDisable(GL_TEXTURE_1D);
                glBindTexture(GL_TEXTURE_1D, 0);
                glUseProgram(prev_program);
            }
            glDisable(GL_CULL_FACE);
        }
    }
    glPopMatrix();
}

void renderer(int ak, enum render_part fast) {
    int o, p, no;

    if (map[ak].first_obj==-1) return;
    // boucler sur tous les objets
    for (o=map[ak].first_obj; o!=-1; o=obj[o].next) {
        if ((fast==1 && obj[o].type!=TYPE_CLOUD) || fast==2) continue;
        // calcul la position de l'objet dans le rep�re de la cam�ra, ie CamT*(objpos-campos)
        subv3(&obj[o].pos,&obj[0].pos,&obj[o].t);
        mulmtv(&obj[0].rot,&obj[o].t,&obj[o].posc);
        obj[o].distance = norme2(&obj[o].posc);
    }
    // reclasser les objets en R2
    if (fast==0 || fast==3) {   // 1-> pas la peine et 2-> deja fait.
        o=map[ak].first_obj;
        if (obj[o].next!=-1) do {   // le if est utile ssi il n'y a qu'un seul objet
            int n=obj[o].next, ii=o;
            //for (p=0; p!=-1; p=obj[p].next) printf("%d<",p);
            //printf("\n");
            while (ii!=-1 && obj[n].distance<obj[ii].distance) ii=obj[ii].prec;
            if (ii!=o) { // r�ins�re
                obj[o].next=obj[n].next;
                if (obj[o].next!=-1) obj[obj[o].next].prec=o;
                obj[n].prec=ii;
                if (ii==-1) {
                    obj[n].next=map[ak].first_obj;
                    map[ak].first_obj=n;
                    obj[obj[n].next].prec=n;
                } else {
                    obj[n].next=obj[ii].next;
                    obj[ii].next=n;
                    obj[obj[n].next].prec=n;
                }
            } else o=obj[o].next;
        } while (o!=-1 && obj[o].next!=-1);
    }
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glFrustum(
        -1, 1,
        -(GLdouble)win_height/win_width, (GLdouble)win_height/win_width,
        1, 10000
    );
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(1, -1, -1);
    // affichage des ombres
    o=map[ak].first_obj; no=0;
    if (fast!=1) do {
        float z;
        char aff=norme2(&obj[o].t)<TILE_LEN*TILE_LEN*4;
        if (obj[o].aff && (fast==3 || (fast==0 && (obj[o].type==TYPE_CAR || obj[o].type==TYPE_TANK || obj[o].type==TYPE_LIGHT || obj[o].type==TYPE_DECO || obj[o].type==TYPE_DEBRIS)) || (fast==2 && (obj[o].type==TYPE_PLANE || obj[o].type==TYPE_ZEPPELIN || obj[o].type==TYPE_INSTRUMENTS || obj[o].type==TYPE_BOMB)))) {
            mulmt3(&oL[no], &obj[o].rot, &light);
#define DISTLUM 300.
            mulv(&oL[no].x,DISTLUM);
            mulv(&oL[no].y,DISTLUM);
            if (aff && (z=z_ground(obj[o].pos.x,obj[o].pos.y, true))>obj[o].pos.z-500) {
                for (p=0; p<mod[obj[o].model].nbpts[1]; p++) {
                    struct vector pts3d;
                    mulmv(&obj[o].rot, &mod[obj[o].model].pts[1][p], &pts3d);
                    addv(&pts3d,&obj[o].pos);
                    pts3d.x+=pts3d.z-z;
                    pts3d.z=z;
                    subv(&pts3d,&obj[0].pos);
                    mulmtv(&obj[0].rot,&pts3d,&pts3d);
                    if (pts3d.z >0) pts[p].v = pts3d;
                    else pts[p].v.x = MAXINT;
                }
                for (p=0; p<mod[obj[o].model].nbfaces[1]; p++) {
                    if (
                        scalaire(&mod[obj[o].model].fac[1][p].norm,&oL[no].z)<=0 && pts[mod[obj[o].model].fac[1][p].p[0]].v.x != MAXINT
                        && pts[mod[obj[o].model].fac[1][p].p[1]].v.x != MAXINT
                        && pts[mod[obj[o].model].fac[1][p].p[2]].v.x != MAXINT
                    ) {
                        polyflat(
                            &pts[mod[obj[o].model].fac[1][p].p[0]].v,
                            &pts[mod[obj[o].model].fac[1][p].p[1]].v,
                            &pts[mod[obj[o].model].fac[1][p].p[2]].v,
                            (struct pixel){ .r = 0, .g = 0, .b = 0}
                        );
                    }
                }
            }
        }
        o=obj[o].next; no++;
    } while (o!=-1);
    if (no>MAXNO) printf("ERROR ! NO>MAXNO AT ak=%d (no=%d)\n",ak,no);
    // affichage dans l'ordre du Z
    o=map[ak].first_obj; no=0;
    // FIXME: so we need map[ak].last_obj as well
    if (fast!=1) while (obj[o].next!=-1 /*&& (viewall || obj[obj[o].next].distance<TL2)*/) { o=obj[o].next; no++; }
    do {
        if (
            fast==3 ||
            (fast==1 && obj[o].type==TYPE_CLOUD) ||
            (fast==0 && (obj[o].type==TYPE_CAR || obj[o].type==TYPE_TANK || obj[o].type==TYPE_LIGHT || obj[o].type==TYPE_DECO || obj[o].type==TYPE_DEBRIS)) ||
            (fast==2 && (obj[o].type==TYPE_PLANE || obj[o].type==TYPE_ZEPPELIN || obj[o].type==TYPE_SMOKE || obj[o].type==TYPE_SHOT || obj[o].type==TYPE_BOMB || obj[o].type==TYPE_INSTRUMENTS || obj[o].type==TYPE_CLOUD))
        ) {
            if (obj[o].aff && obj[o].posc.z>-mod[obj[o].model].rayon) { // il faut d�j� que l'objet soit un peu devant la cam�ra et que ce soit pas un objet � passer...
                if (obj[o].type == TYPE_CLOUD || obj[o].type == TYPE_SMOKE || obj[o].type == TYPE_LIGHT) {
                    glMatrixMode(GL_PROJECTION);
                    glPushMatrix();
                    glLoadIdentity();
                    glOrtho(0, win_width, win_height, 0, -1, 1);
                    glMatrixMode(GL_MODELVIEW);
                    glPushMatrix();
                    glScalef(1, -1, -1);
                    render_sphere(o);
                    glPopMatrix();
                    glMatrixMode(GL_PROJECTION);
                    glPopMatrix();
                    glMatrixMode(GL_MODELVIEW);
                } else {
                    render_obj(o, no);
                }
            }
        }
        if (fast!=1) { o=obj[o].prec; no--; } else o=obj[o].next;
    } while (o!=-1);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static bool to_camera(struct vector const *v3d, struct vect2d *v2d)
{
    struct vector vc = *v3d;
    struct vector vc_c;
    subv(&vc, &obj[0].pos);
    mulmtv(&obj[0].rot, &vc, &vc_c);
    if (vc_c.z > 0.) {
        proj(v2d, &vc_c);
        return true;
    }
    return false;
}

#ifdef VEC_DEBUG
struct vector debug_vector[NB_DBG_VECS][2];
void draw_debug(void)
{
    static int debug_vector_color[] = {
        0xFF0000, 0x00FF00, 0x0000FF,
        0xC0C000, 0xC000C0, 0x00C0C0,
    };

    for (unsigned v = 0; v < ARRAY_LEN(debug_vector); v++) {
        struct vect2d pts2d[2];
        if (to_camera(debug_vector[v]+0, pts2d+0) && to_camera(debug_vector[v]+1, pts2d+1)) {
            drawline(pts2d+0, pts2d+1, debug_vector_color[v % ARRAY_LEN(debug_vector_color)]);
        }
    }
}
#endif

void draw_target(struct vector p, int c)
{
    struct vect2d p2d;
    if (! to_camera(&p, &p2d)) return;
    struct vect2d const min = { .x = p2d.x - 10, .y = p2d.y - 10 };
    struct vect2d const max = { .x = p2d.x + 10, .y = p2d.y + 10 };
    draw_rectangle(&min, &max, c);
}

void draw_mark(struct vector p, int c)
{
    // TODO: a cross on the floor
    draw_target(p, c);
}
