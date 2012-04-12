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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <AL/al.h>
#include <AL/alc.h>
#include "sound.h"
#include "file.h"

//#define PRINT_DEBUG

static bool with_sound;
static ALCdevice *dev;
static ALCcontext *ctx;

static ALuint buffers[NB_SAMPLES];
static bool buffer_looping[NB_SAMPLES]; // tells whether the sample is supposed to loop
static float buffer_gain[NB_SAMPLES];
static ALuint sources[NB_VOICES];
static ALuint last_played[NB_VOICES];
static struct vector listener_pos;
static struct {
    enum snd_sample samp;  // or ~0 if unset
    float pitch;
    struct vector saved_pos;   // copy of given pos in case it was not an attach
    struct vector const *pos;
    bool relative;
    bool anchored;
} play[NB_VOICES];  // to store playing sounds until listener's position is known

#define MAX_DIST (40. * ONE_METER)

struct vector voices_in_my_head = { 0., 1., 0. };  // upstairs...

static char const *al_strerror(ALenum err)
{
    switch (err) {
        case AL_NO_ERROR:          return "No Error";
        case AL_INVALID_NAME:      return "Invalid Name";
        case AL_INVALID_ENUM:      return "Invalid Enum";
        case AL_INVALID_VALUE:     return "Invalid Value";
        case AL_INVALID_OPERATION: return "Invalid Operation";
        case AL_OUT_OF_MEMORY:     return "Out Of Memory";
    }
    return "Unknown Error";
}

static void *load_file(char const *fn, size_t *size)
{
    FILE *in = file_open(fn, DATADIR, "r");
    if (! in) {
        goto exit0;
    }

    if (0 != fseek(in, 0, SEEK_END)) {
        fprintf(stderr, "Cannot fseek(%s): %s\n", fn, strerror(errno));
        goto exit1;
    }

    long sz = ftell(in);
    if (sz < 0) {
        fprintf(stderr, "Cannot ftell(%s): %s\n", fn, strerror(errno));
        goto exit1;
    }

    rewind(in);

    if (size) *size = sz;

    uint8_t *buf = malloc(sz);
    if (! buf) {
        fprintf(stderr, "Cannot malloc(%ld)\n", sz);
        goto exit1;
    }

    size_t read = fread(buf, 1, sz, in);
    if (read < (size_t)sz) {
        fprintf(stderr, "Cannot read %ld bytes from %s: %s\n", sz, fn, strerror(errno));
        goto exit2;
    }

    (void)fclose(in);
    return buf;

exit2:
    free(buf);
exit1:
    (void)fclose(in);
exit0:
    return NULL;
}

static ALenum al_format(unsigned nb_channels, unsigned bits_per_sample)
{
    switch ((nb_channels << 8) | bits_per_sample) {
        case 0x108: return AL_FORMAT_MONO8;
        case 0x110: return AL_FORMAT_MONO16;
        case 0x208: return AL_FORMAT_STEREO8;
        case 0x210: return AL_FORMAT_STEREO16;
    }
    assert(!"Unsuported wave format");
}

// Load a wave file. Supported format include mono/stereo 8 or 16 bits/sample
static int load_wave(enum snd_sample samp, char const *fn, bool loop, float gain)
{
    if (! with_sound) return 0;

    ALenum err;

    struct wave {
        // File header
        char riff[4];
        uint32_t riff_size;
        char wave[4];
        // Format section
        char fmt[4];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t nb_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        // Data section
        char data[4];
        uint32_t data_size;
        uint8_t samples[];
    } __attribute__((__packed__));

    size_t sz;
    struct wave *wave = load_file(fn, &sz);
    if (! wave) goto exit0;

    // Sanity checks
    // FIXME: reorder bytes if we are a bigendian
    if (
        sz < sizeof(struct wave) ||
        wave->riff_size != sz - 8 ||
        wave->riff_size != 4 + (8 + wave->fmt_size) + (8 + wave->data_size) ||
        0 != strncmp(wave->riff, "RIFF", 4) ||
        0 != strncmp(wave->fmt,  "fmt ", 4) ||
        0 != strncmp(wave->data, "data", 4) ||
        0 != strncmp(wave->wave, "WAVE", 4)
    ) {
        fprintf(stderr, "Not a WAVE file: %s\n", fn);
        goto exit1;
    }

    if (
        wave->fmt_size != 16 ||
        wave->audio_format != 1 ||
        wave->nb_channels < 1 ||
        wave->nb_channels > 2 ||
        (wave->bits_per_sample != 8 &&
         wave->bits_per_sample != 16)
    ) {
        fprintf(stderr, "WAVE type not supported: %s\n", fn);
        goto exit1;
    }

    assert(samp < ARRAY_LEN(buffers));
    alGetError();
    alBufferData(buffers[samp], al_format(wave->nb_channels, wave->bits_per_sample), wave->samples, wave->data_size, wave->sample_rate);

    if ((err = alGetError()) != AL_NO_ERROR) {
        fprintf(stderr, "Cannot alBufferData(%s): %s\n", fn, al_strerror(err));
        goto exit1;
    }

    buffer_looping[samp] = loop;
    buffer_gain[samp] = gain;

    free(wave);
    return 0;
exit1:
    free(wave);
exit0:
    with_sound = false;
    return -1;
}

static int load_raw(enum snd_sample samp, char const *fn, bool loop, float gain)
{
    if (! with_sound) return 0;

    ALenum err;

    size_t sz;
    uint8_t *fbuf = load_file(fn, &sz);
    if (! fbuf) goto exit0;

    assert(samp < ARRAY_LEN(buffers));
    alGetError();
    alBufferData(buffers[samp], AL_FORMAT_MONO8, fbuf, sz, 8000);

    if ((err = alGetError()) != AL_NO_ERROR) {
        fprintf(stderr, "Cannot alBufferData(%s): %s\n", fn, al_strerror(err));
        goto exit1;
    }

    buffer_looping[samp] = loop;
    buffer_gain[samp] = gain;

    free(fbuf);
    return 0;
exit1:
    free(fbuf);
exit0:
    with_sound = false;
    return -1;
}

void playsound(enum snd_voice voice, enum snd_sample samp, float pitch, struct vector const *pos, bool relative, bool anchored)
{
    assert(voice < ARRAY_LEN(sources));
    assert(samp < ARRAY_LEN(buffers));

    if (! with_sound) return;

#   ifdef PRINT_DEBUG
    printf("play_sound(voice=%d, sample=%d, pos=%"PRIVECTOR", %s)\n",
        voice, samp, PVECTOR(*pos), relative ? "relative":"absolute");
#   endif

    // Wait to know listener position before actually playing the sample
    // TODO: store up to N sounds per voice and choose the closest ones in do_play ?
    play[voice].samp = samp;
    play[voice].pitch = pitch;
    play[voice].relative = relative;
    play[voice].anchored = anchored;
    if (anchored) {
        play[voice].pos = pos;
    } else {
        play[voice].saved_pos = *pos;
        play[voice].pos = &play[voice].saved_pos;
    }
}

static void do_play(enum snd_voice voice)
{
    ALenum err;

    enum snd_sample const samp = play[voice].samp;
    if (samp >= NB_SAMPLES) return;
    float const pitch = play[voice].pitch;
    struct vector const *pos = play[voice].pos;
    bool const relative = play[voice].relative;
#   ifdef PRINT_DEBUG
    printf("actualy play sample %d, pos=%"PRIVECTOR", %s\n", samp, PVECTOR(*pos), relative ? "relative":"absolute");
#   endif

    struct vector d = *pos;
    if (! relative) subv(&d, &listener_pos);
    if (norme2(&d) > MAX_DIST*MAX_DIST) {
#       ifdef PRINT_DEBUG
        printf("...too far (dist=%f)!\n", norme(&d));
#       endif
        return;
    }

    bool const already_playing = buffer_looping[samp] && last_played[voice] == samp;
    alGetError();
    // If the sound is a loop and we aleady play this sound do not stop/restart the sound
    if (! already_playing) {
        alSourceStop(sources[voice]);
        if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot stop source %d: %s\n", voice, al_strerror(err));

        alSourcei(sources[voice], AL_BUFFER, buffers[samp]);
        if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source buffer for sample %d: %s\n", samp, al_strerror(err));

        alSourcei(sources[voice], AL_LOOPING, buffer_looping[samp]);
        if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source looping for voice %d: %s\n", voice, al_strerror(err));
    }

    alSourcef(sources[voice], AL_GAIN, buffer_gain[samp]);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source gain for voice %d: %s\n", voice, al_strerror(err));

    alSourcef(sources[voice], AL_PITCH, pitch);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source pitch %f: %s\n", pitch, al_strerror(err));

    alSource3f(sources[voice], AL_POSITION, pos->x, pos->y, pos->z);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source position at %f,%f,%f: %s\n", pos->x, pos->y, pos->z, al_strerror(err));

    alSourcei(sources[voice], AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set source base to %s: %s\n", relative?"relative":"absolute", al_strerror(err));

    if (! already_playing) {
        alSourcePlay(sources[voice]);
        if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot play source %d: %s\n", voice, al_strerror(err));

        last_played[voice] = samp;
    }
}

void update_listener(struct vector const *pos, struct vector const *velocity, struct matrix const *rot)
{
    if (! with_sound) return;

    ALenum err;

#   ifdef PRINT_DEBUG
    printf("update_listener(pos=%"PRIVECTOR", vel=%"PRIVECTOR")\n", PVECTOR(*pos), PVECTOR(*velocity));
#   endif

    listener_pos = *pos;

    alListener3f(AL_POSITION, pos->x, pos->y, pos->z);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set listener's position to %f,%f,%f: %s\n", pos->x, pos->y, pos->z, al_strerror(err));

    alListener3f(AL_VELOCITY, velocity->x, velocity->y, velocity->z);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set listener's velocity to %f,%f,%f: %s\n", velocity->x, velocity->y, velocity->z, al_strerror(err));

    ALfloat at_up[6] = {
        rot->z.x, rot->z.y, rot->z.z,   // "at" vector
        rot->y.x, rot->y.y, rot->y.z,   // "up" vector
    };
    alListenerfv(AL_ORIENTATION, at_up);
    if ((err = alGetError()) != AL_NO_ERROR) fprintf(stderr, "Cannot set listener's orientation to %f,%f,%f / %f,%f,%f: %s\n", at_up[0], at_up[1], at_up[2], at_up[3], at_up[4], at_up[5], al_strerror(err));

    // play new sounds
    for (unsigned v = 0; v < ARRAY_LEN(play); v++) {
        if (play[v].samp >= NB_SAMPLES) continue;
        do_play(v);
        if (! play[v].anchored) play[v].samp = ~0;
    }
}

void sound_suspend(void)
{
    if (! with_sound) return;

    alcSuspendContext(ctx);
}

void sound_resume(void)
{
    if (! with_sound) return;

    alcProcessContext(ctx);
}

/*
 * Init
 */

int sound_init(bool with_sound_)
{
    ALenum err;

    with_sound = with_sound_;
    if (! with_sound) return 0;

    dev = alcOpenDevice(NULL);
    if (! dev) {
        fprintf(stderr, "Cannot aclOpenDevice()\n");
        goto exit0;
    }

    static ALCint attrs[] = {
        ALC_MONO_SOURCES, ARRAY_LEN(sources),
        ALC_STEREO_SOURCES, 0,
        0, 0
    };
    ctx = alcCreateContext(dev, attrs);
    if (! ctx) {
        fprintf(stderr, "Cannot alcCreateContext()\n");
        goto exit1;
    }

    alcMakeContextCurrent(ctx);

    alGetError();
    alGenBuffers(ARRAY_LEN(buffers), buffers);
    alGenSources(ARRAY_LEN(sources), sources);
    if ((err = alGetError()) != AL_NO_ERROR) {
        fprintf(stderr, "Cannot genBuffers(): %s\n", al_strerror(err));
        goto exit2;
    }
    for (unsigned s = 0; s < ARRAY_LEN(sources); s++) {
        alSourcef(sources[s], AL_MAX_DISTANCE, MAX_DIST);
        alSourcef(sources[s], AL_REFERENCE_DISTANCE, 2. * ONE_METER);
    }
    for (unsigned s = 0; s < ARRAY_LEN(last_played); s++) {
        last_played[s] = ~0U;
    }
    for (unsigned v = 0; v < ARRAY_LEN(play); v++) {
        play[v].samp = NB_SAMPLES;
    }
    alSpeedOfSound(34330.); // our unit of distance is approx the cm

    // Load all samples
    load_raw(SAMPLE_PRESENT,"snd/pingouin.raw", false, 1.);
    load_raw(SAMPLE_BIPINTRO,"snd/bipintro.raw", false, 1.);
    load_wave(SAMPLE_SHOT,"snd2/shot1.wav", false, 1.);
    load_raw(SAMPLE_GEAR_DN,"snd/gear_dn.raw", false, 1.);
    load_raw(SAMPLE_GEAR_UP,"snd/gear_up.raw", false, 1.);
    load_raw(SAMPLE_SCREETCH,"snd/screetch.raw", false, 1.);
    load_wave(SAMPLE_LOW_SPEED, "snd2/taxi.wav", true, .4);
    load_wave(SAMPLE_MOTOR, "snd2/spit2.wav", true, .7);
    load_raw(SAMPLE_HIT,"snd/hit.raw", false, 1.);
    load_raw(SAMPLE_MESSAGE,"snd/message.raw", false, 1.);
    load_wave(SAMPLE_EXPLOZ,"snd2/bombhit.wav", false, 1.);
    load_wave(SAMPLE_BOMB_BLAST,"snd2/boum.wav", false, 1.);
    load_raw(SAMPLE_TOLE,"snd/tole.raw", false, 1.);
    load_raw(SAMPLE_BIPBIP,"snd/bipbip.raw", false, 1.);
    load_raw(SAMPLE_BIPBIP2,"snd/bipbip2.raw", false, 1.);
    load_raw(SAMPLE_BIPBIP3,"snd/bipcarte.raw", false, 1.);
    load_raw(SAMPLE_FEU,"snd/feu.raw", true, 1.);
    load_raw(SAMPLE_TARATATA,"snd/taratata.raw", false, 1.);
    load_raw(SAMPLE_ALLELUIA,"snd/alleluia.raw", false, 1.);
    load_raw(SAMPLE_ALERT,"snd/alert.raw", false, 1.);
    load_raw(SAMPLE_PAIN,"snd/pain.raw", false, 1.);
    load_wave(SAMPLE_DEATH,"snd2/death.wav", false, 1.);
    load_wave(SAMPLE_BRAVO,"snd2/bravo.wav", false, 1.);

    printf("Sound OK\n");
    return 0;

exit2:
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
exit1:
    alcCloseDevice(dev);
exit0:
    with_sound = false;
    return -1;
}

void sound_fini(void)
{
    if (! with_sound) return;

    alDeleteSources(ARRAY_LEN(sources), sources);
    alDeleteBuffers(ARRAY_LEN(buffers), buffers);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
}
