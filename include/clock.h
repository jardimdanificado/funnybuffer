#ifndef WAGNOSTIC_CLOCK_H
#define WAGNOSTIC_CLOCK_H

#include <stdint.h>

#define WCLOCK_EXTENSION "std:clock"

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    float    delta;
} wclock_t;

#endif /* WAGNOSTIC_CLOCK_H */
