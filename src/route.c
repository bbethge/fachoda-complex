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
#include <values.h>
#include "map.h"
#include "robot.h"

#define NBMAXROUTE 5000 /* <=65534 ! */
route_s *route;
int routeidx=0;
int NbElmLim;
int EndRus=-1, EndMotorways=-1, EndRoads=-1;
int largroute[3]={100,70,50};
int actutype=0;
short (*map2route)[NBREPHASH];  // table de hash

void hashroute() {
    int i,j, nk, missed=0;
    int nbe[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // nbr de cases utilis�es pour chaque ligne de la table de hash
    // (une ligne = d�teemin�e par les 2 bits venant du num�ro de ak)
    short hi;
    for (i=0; i<routeidx; i++) {
        if (route[i].ak==-1) continue;
        hi=(route[i].ak&(3<<NWMAP))>>(NWMAP-2);
        hi|=(route[i].ak&3);
        if (!(map[route[i].ak].has_road)) { // premi�re route dans cette kase
            // on peut donc fixer arbitrairement le code nk
            nk=nbe[hi]++;
            if (nk>=(1<<7)) {
                printf("HI=%d\n",hi);
                printf("ERROR: Hash Table too narrow\n");
                exit(-1);
            }
            hi|=nk<<4;
            map2route[hi][0]=(short)i;
            map[route[i].ak].has_road = 1;
            map[route[i].ak].submap = nk;
        } else {    // il y a d�j� une route dans cette kase
            // donc on ne peut pas fixer nk comme ca nous arrange
            // donc il faut rajouter cet elemnt de route � la meme
            // position dans le hash
            nk = map[route[i].ak].submap;
            hi|=nk<<4;
            for (j=0; map2route[hi][j]!=-1 && j<4; j++);
            if (j==0) printf("ERROR CONSTRUCTING HASH TABLE !\n");  // il y a deja un elmt ou pas ?
            if (j<4) map2route[hi][j]=(short)i;
            else missed++;
        }
    }
    for (i=j=0; i<16; i++) j+=nbe[i];
//  printf("Hash table fullness : %.0f %%\nMissed elements : %.0f %%\n",j*100./(float)(1<<NHASH),missed*100./j);
}

void initroute() {
    int i,j;
    route=(route_s*)malloc(NBMAXROUTE*sizeof(route_s));
    map2route=malloc((1<<NHASH)*NBREPHASH*sizeof(short));
    for (i=0; i<(1<<NHASH); i++)
        for (j=0; j<NBREPHASH; j++)
            map2route[i][j]=-1;

}
void endinitroute() {
    route=(route_s*)realloc(route,routeidx*sizeof(route_s));
}
float oldcap,curcap,bestcap;
float note(vector *a, vector *f, vector *i) {
    float n,dist,zs;
    vector v;
    if ((zs=z_ground(a->x,a->y, false))<3000 || zs>11500) n=MAXFLOAT;
    else {
        copyv(&v,a);
        subv(&v,i);
        if (v.y!=0 || v.x!=0) {
            curcap=cap(v.x,v.y);
            copyv(&v,f);
            subv(&v,i);
            dist=norme(&v);
            n=curcap-cap(v.x,v.y);
            if (n<-M_PI) n+=2*M_PI;
            else if (n>M_PI) n-=2*M_PI;
            n=fabs(n);
            n+=(dist>ECHELLE*10?.0015:.0025)*fabs(zs-z_ground(i->x,i->y, false));
            n+=0.003*fabs(zs-z_ground(f->x,f->y, false))/(dist+100);
            if (oldcap!=MAXFLOAT) {
                float dc=curcap-oldcap;
                if (dc<-M_PI) dc+=2*M_PI;
                else if (dc>M_PI) dc-=2*M_PI;
                dc=fabs(dc);
                n+=1.1*dc;
            }
        } else n=MAXFLOAT;
    }
    return n;
}
void nxtrt(vector i, vector *f, int lastd) {
    // routeidx pointe sur une route dont l'origine est mise, mais
    // pointant sur rien
    int d, bestd=0, intens;
    float bestnote=MAXFLOAT, notecur, dist, s;
    pixel coul;
    vector r,besta,u;
    float tmp;
    copyv(&r,f);
    subv(&r,&i);
    dist=norme(&r);
    if (NbElmLim<0 || dist<ECHELLE || routeidx>=NBMAXROUTE-1) {
        routeidx++;
        route[routeidx].ak=-1;  // mark fin de route
        routeidx++;
        return;
    }
    NbElmLim--;
    akref(route[routeidx].ak,&r);
//  printf("nxtrt: ak=%d  akref=%f %f  i=%f %f\n",route[routeidx].ak,r.x,r.y,route[routeidx].i.x,route[routeidx].i.y);
//  printf("nxtrt : from %f %f - dist=%f\n",i.x,i.y,dist);
    for (d=0; d<4; d++) {
        // d=cot� parcouru : 0=sud, 1=ouest, 2=nord, 3=ouest
        if (d==(lastd^2)) continue;
        for (s=ECHELLE/5; s<ECHELLE*4/5; s+=ECHELLE/10) {
            float ll=d>1?ECHELLE:0;
            vector a;
            a.x=(d&1?ll:s)+r.x;
            a.y=(d&1?s:ll)+r.y;
            a.z=z_ground(a.x,a.y, false);
            if ((notecur=note(&a,f,&i))<bestnote) {
                bestnote=notecur;
                bestd=d;
                copyv(&besta,&a);
                bestcap=curcap;
            }
        }
    }
    if (bestnote>1000) {
        route[routeidx].ak=-1;
        routeidx++;
        return;
    }
    oldcap=bestcap;
//  printf(" found this way : dir=%d, leading to %f %f (note=%f)\n",bestd, besta.x,besta.y,bestnote);
    routeidx++;
    copyv(&route[routeidx].i,&besta);
    subv3(&route[routeidx-1].i,&route[routeidx].i,&u);
    tmp=u.x; u.x=u.y; u.y=-tmp; u.z=0;
    renorme(&u);
    mulv(&u,largroute[actutype]);
    addv3(&route[routeidx].i,&u,&route[routeidx].i2);
    route[routeidx].ak=route[routeidx-1].ak;
    *((int*)&route[routeidx-1].e.c)=0x4A6964;   // couleur par d�faut
    switch (bestd) {
    case 0:
        route[routeidx].ak-=WMAP;
        d=route[routeidx].ak<WMAP;
        break;
    case 1:
        route[routeidx].ak-=2;
    case 3:
        route[routeidx].ak+=1;
        d=(route[routeidx].ak&WMAP)!=(route[routeidx-1].ak&WMAP);
        break;
    case 2:
        route[routeidx].ak+=WMAP;
        d=route[routeidx].ak>=WMAP<<(NWMAP-1);
        break;
    }
    if (d) {
//      printf("Interrputing road\n");
        route[routeidx].ak=-1;
        routeidx++;
        return;
    }
    // colore la route
    intens=((map[route[routeidx].ak].z-map[(route[routeidx].ak-1)&((WMAP<<NWMAP)-1)].z))+32+64;
    if (intens<80) intens=80;
    else if (intens>117) intens=117;
    if (EndMotorways==-1) {coul.r=120; coul.g=150; coul.b=130; }
    else if (EndRoads==-1) {coul.r=90; coul.g=130; coul.b=110; }
    else { coul.r=100; coul.g=120; coul.b=70; }
    route[routeidx-1].e.c.r=(coul.r*intens)>>7;
    route[routeidx-1].e.c.g=(coul.g*intens)>>7;
    route[routeidx-1].e.c.b=(coul.b*intens)>>7;
    if (route[routeidx-1].e.c.r<20) route[routeidx-1].e.c.r=20; // pour �viter les swaps
    if (route[routeidx-1].e.c.g<20) route[routeidx-1].e.c.g=20;
    if (route[routeidx-1].e.c.b<20) route[routeidx-1].e.c.b=20;
    // avance le chantier
    akref(route[routeidx].ak,&r);
    nxtrt(besta,f,bestd);
}

void traceroute(vector *i, vector *f) {
    if (routeidx>=NBMAXROUTE-1) return;
    route[routeidx].ak=akpos(i);
    copyv(&route[routeidx].i,i);
    copyv(&route[routeidx].i2,i);
    oldcap=MAXFLOAT;
    nxtrt(*i,f,-1);
}

void prospectroute(vector *i, vector *f) {
    int deb=routeidx, bestfin=MAXINT;
    int j,k;
    int nbelmlim;   // nb d'element au dela duquel ca vaut pas le coup
    vector p1,p2, bestp1, bestp2, v;
    copyv(&p1,i);
    copyv(&p2,f);
    copyv(&v,f);
    subv(&v,i);
    nbelmlim=8.*norme(&v)/ECHELLE;
    if (EndMotorways==-1) actutype=0;
    else if (EndRoads==-1) actutype=1;
    else actutype=2;
    for (j=0; j<10; j++) {
        if (j) {
            randomv(&v);
            mulv(&v,ECHELLE*(EndRoads==-1?2:0.00001));
            addv(&p1,&v);
            p1.z=z_ground(p1.x,p1.y, false);
            randomv(&v);
            mulv(&v,ECHELLE*(EndRoads==-1?2:3));
            addv(&p2,&v);
            p2.z=z_ground(p2.x,p2.y, false);
        }
        NbElmLim=nbelmlim;
        traceroute(&p1,&p2);
        k=routeidx-2;
        if (k>=0) {
//          akref(route[k].ak,&v);
            v = route[k].i;
            subv(&v, f);
            if (
                routeidx < bestfin &&
                norme(&v) < 2.*ECHELLE
            ) {
                bestp1 = p1;
                bestp2 = p2;
                bestfin = routeidx;
            }
        }
        routeidx=deb;
    }
    if (bestfin-routeidx<nbelmlim) {
        NbElmLim=nbelmlim;
        traceroute(&bestp1,&bestp2);
    }
}


