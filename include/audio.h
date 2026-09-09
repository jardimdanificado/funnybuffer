#ifndef WAGNOSTIC_AUDIO_H
#define WAGNOSTIC_AUDIO_H

#include <stdint.h>

#define WAUDIO_EXTENSION "audio"
#define WAUDIO_VERSION   1

#define WAUDIO_F32  1
#define WAUDIO_S16  2

typedef struct {
    uint32_t version;
    uint32_t size;

    uint32_t sample_rate;
    uint32_t channels;
    uint32_t format;

    uint32_t buffer;
    uint32_t capacity;

    uint32_t write;
    uint32_t read;
} waudio_t;

#endif /* WAGNOSTIC_AUDIO_H */
