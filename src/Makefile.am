## Process this file with automake to produce Makefile.in
AM_CPPFLAGS = -DDATADIR='"$(pkgdatadir)"' -D_GNU_SOURCE
bin_PROGRAMS = fachoda getkeyname
SUBDATADIRS = \
    snd snd2 \
    arbre2 base1 base1light bovide chalet corsair corsairlight \
    d501 d501light eglise egliselight ferme maison \
    moshito moshitolight moulin reverbere \
    snophill snoplight sol spitflame spitflamelight \
    tank tanklight toyvion toyvionlight tracteur usine \
    van vehic vion1 vion2 zeppelin
nobase_dist_pkgdata_DATA = \
    bigfont.tga complex.jpg font.tga icon.png metal50_50.tga wood50_50.tga \
    $(shell find $(SUBDATADIRS) '!' -name '*.dxf')
dist_doc_DATA = ../COPYING ../GPL.txt ../README.md .fachodarc

fachoda_SOURCES = \
    renderer.c heightfield.c model.c radio.c robot.c \
    instruments.c txt.c physics.c mapping.c \
    control.c sun.c map.c sound.c ravages.c \
    present.c keycodes.c roads.c \
    init.c intro.c main.c video_sdl.c world.c \
    gtime.c file.c config.c
fachoda_CFLAGS = $(SDL_CFLAGS) $(GL_CFLAGS)
fachoda_LDFLAGS = $(SDL_LIBS) $(OPENAL_LIBS) $(GL_LIBS)

getkeyname_SOURCES = getkeyname.c
getkeyname_CFLAGS = $(SDL_CFLAGS)
getkeyname_LDFLAGS = $(SDL_LIBS)
