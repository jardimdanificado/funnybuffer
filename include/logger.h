#ifndef WAGNOSTIC_LOGGER_H
#define WAGNOSTIC_LOGGER_H

#include <stdint.h>

#define WLOGGER_EXTENSION "logger"
#define WLOGGER_VERSION   1

typedef struct {
    uint32_t version;     /* 1 */
    uint32_t size;        /* sizeof(wlogger_t) = 20 */
    uint32_t buffer;      /* WASM memory pointer to UTF-8 text buffer */
    uint32_t capacity;    /* Buffer capacity in bytes */
    uint32_t length;      /* Length of text written by ROM (host clears to 0 after printing) */
} wlogger_t;

#endif /* WAGNOSTIC_LOGGER_H */
