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
#include <assert.h>
#include "proto.h"
#include "file.h"
#include "video_sdl.h"
#include "SDL_opengl.h"

unsigned char num[10][8]= {
    { 0x3E,0x41,0x41,0x41,0x41,0x41,0x41,0x3E },        // 0
    { 0x3C,0x18,0x18,0x18,0x18,0x38,0x18,0x08 },        // 1
    { 0x7F,0x39,0x18,0x0C,0x06,0x43,0x63,0x3E },        // 2
    { 0x3E,0x63,0x43,0x03,0x07,0x43,0x63,0x3E },        // 3
    { 0x0F,0x06,0x06,0x3F,0x36,0x30,0x30,0x30 },
    { 0x3E,0x47,0x03,0x63,0x7E,0x60,0x61,0x7F },        // 5
    { 0x1C,0x36,0x63,0x73,0x6C,0x60,0x63,0x3E },
    { 0x60,0x60,0x70,0x38,0x0E,0x03,0x43,0x7F },        // 7
    { 0x3E,0x63,0x63,0x63,0x3E,0x63,0x63,0x3E },
    { 0x0C,0x06,0x03,0x1F,0x33,0x63,0x63,0x3E }     // 9
};
void pnumchar(int n, int x, int y, int c) {
    glColor3ub((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF);
    glRasterPos2i(x, y+8);
    glBitmap(8, 8, 0, 0, 0, 0, num[n]);
}
void pnum(int n, int x, int y, int c, int just) {
    int sig=2*(n>=0)-1;
    int m=n;
    if (just==1) { // justifi� � gauche
        if (sig==1) x-=8;
        while (m!=0) { x+=8; m/=10; };
    }
    do {
        pnumchar(sig*(n%10),x,y,c);
        x-=8;
        n/=10;
    } while (n!=0);
    if (sig==-1) {
        glColor3ub((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF);
        glRecti(x+2, y+4, x+6, y+5);
    }
}
void pnumchara(int n, int x, int y, int c) {    // a = dans le mapping
    int i, l;
    for (l=7; l>=0; l--, y++) {
        for (i=128; i>=1; i>>=1, x++) if (num[n][l]&i) ((int*)mapping)[x+(y<<8)]=c;
        x-=8;
    }
}
void pnuma(int n, int x, int y, int c, int just) {
    int sig=2*(n>=0)-1;
    int m=n;
    if (just==1) { // justifi� � gauche
        if (sig==1) x-=8;
        while (m!=0) { x+=8; m/=10; };
    }
    while (n!=0) {
        pnumchara(sig*(n%10),x,y,c);
        x-=8;
        n/=10;
    };
    if (sig==-1) for (m=x+2; m<x+6; m++) ((int*)mapping)[m+((y+4)<<8)]=c;
}
uint8_t font[112][10];
GLuint fonttex;
#define SizeCharX 8
int SizeCharY=10;
GLsizei fonttexw, fonttexh;

void loadfont(char *fn, int nx, int ny, int cy) {
    FILE *fil;
    int x,y,fx,fy,i,sx=0,sy=0;
    struct pixel *itmp;
    GLubyte *textmp;
    SizeCharY=cy;
    fonttexw = 1;
    while (fonttexw < SizeCharX) fonttexw <<= 1;
    fonttexh = 1;
    while (fonttexh < cy*nx*ny) fonttexh <<= 1;
    if ((fil=file_open(fn, DATADIR, "r"))==NULL) {
        exit(-1);
    }
    fseek(fil,12,SEEK_SET);
    file_read(&sx, 2, fil);
    file_read(&sy, 2, fil);
    fseek(fil,-sx*sy*sizeof(struct pixel),SEEK_END);
    itmp = malloc(sx*sy*sizeof(*itmp));
    textmp = malloc(fonttexw*fonttexh*sizeof(*textmp));
    file_read(itmp, sx*sy*sizeof(*itmp), fil);
    fclose(fil);
    for (fy=0; fy<ny; fy++)
        for (fx=0; fx<nx; fx++) {
            for (y=0; y<SizeCharY; y++) {
                for (i=0, x=0; x<SizeCharX; x++) {
                    i<<=1;
                    i+=itmp[(fy*cy+y)*sx+SizeCharX*fx+x].g!=0;
                    textmp[SizeCharX*(y+cy*(nx*fy+fx))+x] =
                        itmp[(fy*cy+y)*sx+SizeCharX*fx+x].g;
                }
                font[fy*nx+fx][y]=i;
            }
        }
    free(itmp);
    glGenTextures(1, &fonttex);
    glBindTexture(GL_TEXTURE_2D, fonttex);
    glTexImage2D(
            GL_TEXTURE_2D, 0, GL_ALPHA, fonttexw, fonttexh, 0, GL_ALPHA,
            GL_UNSIGNED_BYTE, textmp);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(textmp);
}
uint8_t *BigFont;
int SizeBigCharY=50, SizeBigCharX=50, SizeBigChar=2500;

void loadbigfont(char *fn) {
    FILE *fil;
    int x,y,fx,sx=0,sy=0;
    struct pixel32 *itmp;
    if ((fil=file_open(fn, DATADIR, "r"))==NULL) {
        exit(-1);
    }
    fseek(fil, 12, SEEK_SET);
    file_read(&sx, 2, fil);
    file_read(&sy, 2, fil);
    SizeBigCharY=sy;
    SizeBigCharX=sx/13;
    SizeBigChar=SizeBigCharX*SizeBigCharY;
    fseek(fil,-sx*sy*sizeof(struct pixel32),SEEK_END);
    itmp = malloc(sx*sy*sizeof(*itmp));
    file_read(itmp, sx*sy*sizeof(*itmp), fil);
    fclose(fil);
    BigFont = malloc(sx*sy*sizeof(*BigFont));
    for (fx=0; fx<13; fx++) {
        for (y=0; y<SizeBigCharY; y++) {
            for (x=0; x<SizeBigCharX; x++)
                BigFont[fx*SizeBigChar+y*SizeBigCharX+x]=itmp[fx*SizeBigCharX+y*sx+x].u;
        }
    }
    free(itmp);
}

static int addsat_byte(int b1, int b2, int shft) {
    int const mask = 0xff<<shft;
    int const a = (b1 & mask) + (b2 & mask);
    return a > mask ? mask : a;
}
static int subsat_byte(int b1, int b2, int shft) {
    int const mask = 0xff<<shft;
    int const a = (b1 & mask) - (b2 & mask);
    return a > mask /* wrap */ || a < (1<<shft) ? 0 : a;
}
void MMXAddSatC(int *dst, int coul) {
    int v = *dst;
    *dst = (
        addsat_byte(v, coul, 0) |
        addsat_byte(v, coul, 8) |
        addsat_byte(v, coul, 16)
    );
}
static void MMXSubSatC(int *dst, int coul) {
    int v = *dst;
    *dst = (
        subsat_byte(v, coul, 0) |
        subsat_byte(v, coul, 8) |
        subsat_byte(v, coul, 16)
    );
}
void MMXAddSat(int *dst, int byte) {
    int b = byte & 0xff;
    MMXAddSatC(dst, b | (b<<8) | (b<<16));
}
void MMXSubSat(int *dst, int byte) {
    int b = byte & 0xff;
    MMXSubSatC(dst, b | (b<<8) | (b<<16));
}
void MMXAddSatInt(int *dst, int byte, int n)
{
    while (n--) MMXAddSat(dst++, byte);
}

void pbignumchar(int n, int x, int y, int coul) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_CONSTANT_COLOR, GL_ONE);
    glBlendColor(
            ((coul>>16)&0xFF)/255.f, ((coul>>8)&0xFF)/255.f, (coul&0xFF)/255.f,
            1);
    glRasterPos2i(x, y);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDrawPixels(
            SizeBigCharX, SizeBigCharY, GL_LUMINANCE, GL_UNSIGNED_BYTE,
            BigFont+n*SizeBigChar);
    assert(glGetError()==GL_NO_ERROR);
    glBlendColor(0, 0, 0, 0);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
}
void pbignum(int n, int x, int y, int just, char tot, char dolsig) {
    int m, c;
    uint8_t sig=0;
    if (n<0) { sig=1; n=-n; }
    if (tot) c=0x7F7F7F;
    else {
        if (sig) c=0xA02020;
        else c=0x2020A0;
    }
    m=n;
    if (just==1) { // justifi� � gauche
        while (m!=0) { x+=SizeBigCharX; m/=10; };
    } else if (just==2) {
        x-=SizeBigCharX>>1;
        while (m!=0) { x+=SizeBigCharX>>1; m/=10; };
    } else x-=SizeBigCharX;
    while (n!=0) {
        pbignumchar(n%10,x,y,c);
        x-=SizeBigCharX;
        n/=10;
    }
    if (dolsig) pbignumchar(12,x,y,c);
    if (!tot || sig) pbignumchar(10+sig,x-SizeBigCharX,y,c);
}

static int is_printable(int m) {
    return m>=16 && m<=112+16;
}

int TextClipX1=0, TextClipX2=0, TextColfont=0;
void pcharady(int m, int *v, int c, int off) {
    int i, l, y, x;
    assert(is_printable(m));
    for (l=0, y=0; l<10; l++, y+=off) {
        for (x=0, i=128; i>=1; i>>=1, x++) if (font[m-16][l]&i) v[x+y]=c;
        x-=8;
    }
}
void pcharlent(int m, int x, int y, int c) {
    int i, l;
    assert(is_printable(m));
    glBindTexture(GL_TEXTURE_2D, fonttex);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glScalef(1, (float)SizeCharY/fonttexh, 1);
    glBegin(GL_QUADS);
    glColor3ub(0, 0, 0);
    glTexCoord2i(0, m-16); glVertex2i(x+1, y+1);
    glTexCoord2i(1, m-16); glVertex2i(x+SizeCharX+1, y+1);
    glTexCoord2i(1, m-15); glVertex2i(x+SizeCharX+1, y+SizeCharY+1);
    glTexCoord2i(0, m-15); glVertex2i(x+1, y+SizeCharY+1);
    glColor3ub((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF);
    glTexCoord2i(0, m-16); glVertex2i(x, y);
    glTexCoord2i(1, m-16); glVertex2i(x+SizeCharX, y);
    glTexCoord2i(1, m-15); glVertex2i(x+SizeCharX, y+SizeCharY);
    glTexCoord2i(0, m-15); glVertex2i(x, y+SizeCharY);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void pchar(int m, int x, int y, int c) {
    int i, l;
    assert(is_printable(m));
    glBindTexture(GL_TEXTURE_2D, fonttex);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glScalef(1, (float)SizeCharY/fonttexh, 1);
    glBegin(GL_QUADS);
    glColor3ub((TextColfont>>16)&0xFF, (TextColfont>>8)&0xFF, TextColfont&0xFF);
    glTexCoord2i(0, m-16); glVertex2i(x+1, y+1);
    glTexCoord2i(1, m-16); glVertex2i(x+SizeCharX+1, y+1);
    glTexCoord2i(1, m-15); glVertex2i(x+SizeCharX+1, y+SizeCharY+1);
    glTexCoord2i(0, m-15); glVertex2i(x+1, y+SizeCharY+1);
    glColor3ub((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF);
    glTexCoord2i(0, m-16); glVertex2i(x, y);
    glTexCoord2i(1, m-16); glVertex2i(x+SizeCharX, y);
    glTexCoord2i(1, m-15); glVertex2i(x+SizeCharX, y+SizeCharY);
    glTexCoord2i(0, m-15); glVertex2i(x, y+SizeCharY);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void pword(char const *m, int x, int y, int c) {
    do {
        pchar(*m,x,y,c);    // pchar normalement, mais sinon present bug qd les bouttons clippent
        x+=6;
        m++;
    } while (*m!=' ' && is_printable(*m));
}

void pwordlent(char const *m, int x, int y, int c) {
    do {
        pcharlent(*m,x,y,c);
        x+=6;
        m++;
    } while (*m!=' ' && is_printable(*m));
}

void pstr(char const *m, int y, int c) {
    int l,ll,x;
    int sx1=TextClipX1?TextClipX1:0;
    int sx2=TextClipX2?TextClipX2:win_width;
    if ((l=strlen(m)*6)<(sx2-sx1)) x=sx1+((sx2-sx1-l)>>1);
    else {
        l=strlen(m);
        x=sx1;
        do {
            do l--; while(l && m[l]!=' ' && is_printable(m[l]));
            if (l && l*6<sx2) {
                x=sx1+((sx2-sx1-l*6)>>1);
                break;
            }
        } while (l);
    }
    do {
        l=0;
        do l++; while(m[l]!=' ' && is_printable(m[l]));
        if (x+l*6<sx2) {
            pword(m,x,y,c);
        } else {
            y+=SizeCharY+1;
            if ((ll=strlen(m)*6)<(sx2-sx1)) x=sx1+((sx2-sx1-ll)>>1);
            else {
                x=sx1;
                ll=strlen(m);
                do {
                    do ll--; while(ll && m[ll]!=' ' && is_printable(m[ll]));
                    if (ll && ll*6<sx2) {
                        x=sx1+((sx2-sx1-ll*6)>>1);
                        break;
                    }
                } while (ll);
            }
            pword(m,x,y,c);
        }
        x+=l*6+5;
        m+=l;
        while(*m!='\0' && (*m==' ' || !is_printable(*m))) m++;
    } while (*m!='\0');
}

void pstrlent(char const *m, int y, int c) {
    int l,ll,x;
    int sx1=TextClipX1?TextClipX1:0;
    int sx2=TextClipX2?TextClipX2:win_width;
    if ((l=strlen(m)*6)<(sx2-sx1)) x=sx1+((sx2-sx1-l)>>1);
    else {
        l=strlen(m);
        x=sx1;
        do {
            do l--; while(l && m[l]!=' ' && is_printable(m[l]));
            if (l && l*6<sx2) {
                x=sx1+((sx2-sx1-l*6)>>1);
                break;
            }
        } while (l);
    }
    do {
        l=0;
        do l++; while(m[l]!=' ' && is_printable(m[l]));
        if (x+l*6<sx2) {
            pwordlent(m,x,y,c);
        } else {
            y+=SizeCharY+1;
            if ((ll=strlen(m)*6)<(sx2-sx1)) x=sx1+((sx2-sx1-ll)>>1);
            else {
                x=sx1;
                ll=strlen(m);
                do {
                    do ll--; while(ll && m[ll]!=' ' && is_printable(m[ll]));
                    if (ll && ll*6<sx2) {
                        x=sx1+((sx2-sx1-ll*6)>>1);
                        break;
                    }
                } while (ll);
            }
            pwordlent(m,x,y,c);
        }
        x+=l*6+5;
        m+=l;
        while(*m!='\0' && (*m==' ' || !is_printable(*m))) m++;
    } while (*m!='\0');
}

