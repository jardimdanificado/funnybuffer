#ifndef WAGNOSTIC_CLOCK_H
#define WAGNOSTIC_CLOCK_H

#include <stdint.h>

#define WCLOCK_EXTENSION "clock"
#define WCLOCK_VERSION   1

typedef struct {
    uint32_t version;
    uint32_t size;

    uint64_t ticks;
    uint64_t frequency;

    float delta;
} wclock_t;

#endif /* WAGNOSTIC_CLOCK_H */
