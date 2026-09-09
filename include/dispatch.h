#ifndef WAGNOSTIC_DISPATCH_H
#define WAGNOSTIC_DISPATCH_H

#include <stdint.h>

#define WDISPATCH_EXTENSION "std:dispatch"
#define WDISPATCH_VERSION   1

#define WASH_DISPATCH_EXTENSION "wash:dispatch"
#define WASH_DISPATCH_VERSION   1

typedef struct {
    uint32_t version;          /* 1 */
    uint32_t size;             /* sizeof(wdispatch_t) */

    /* Worker Topology */
    uint32_t worker_id;        /* Current worker index (0 .. worker_count - 1) */
    uint32_t worker_count;     /* Total number of active worker threads */

    /* 1D Partition (Arrays, Particles, Audio) */
    uint32_t global_offset;    /* Start index in buffer */
    uint32_t global_length;    /* Number of elements for this worker */
    uint32_t total_elements;   /* Total dataset size */

    /* 2D Partition (Image tiles, Matrix blocks) */
    uint32_t tile_x;           /* Tile left coordinate */
    uint32_t tile_y;           /* Tile top coordinate */
    uint32_t tile_w;           /* Tile width */
    uint32_t tile_h;           /* Tile height */
    uint32_t full_w;           /* Full surface width */
    uint32_t full_h;           /* Full surface height */
    uint32_t stride;           /* Row stride in elements/pixels */

    /* Data Pointer */
    uint32_t data_ptr;         /* WASM memory pointer to primary buffer */
} wdispatch_t;

typedef wdispatch_t wash_dispatch_t;

#endif
