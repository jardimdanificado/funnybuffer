#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

#include <stdint.h>
#include <stddef.h>

#define WAGNOSTIC_VERSION 2

#define WUPDATE_OK      0
#define WUPDATE_EXIT    1
#define WUPDATE_ERROR  -1

#ifdef __cplusplus
extern "C" {
#endif

void *wextension(
    const char *name,
    uint32_t version
);

int32_t wupdate(void);

#ifdef __cplusplus
}
#endif

#endif /* WAGNOSTIC_H */
