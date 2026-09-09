#ifndef WAGNOSTIC_STORAGE_H
#define WAGNOSTIC_STORAGE_H

#include <stdint.h>

#define WSTORAGE_EXTENSION "storage"
#define WSTORAGE_VERSION   1

typedef struct {
    uint32_t version;     /* 1 */
    uint32_t size;        /* sizeof(wstorage_t) = 24 */
    uint32_t data;        /* WASM memory pointer to persistent save data buffer */
    uint32_t capacity;    /* Buffer capacity in bytes */
    uint32_t length;      /* Number of valid bytes in storage */
    uint32_t dirty;       /* ROM sets to 1 to request host flush to disk */
} wstorage_t;

#endif /* WAGNOSTIC_STORAGE_H */
