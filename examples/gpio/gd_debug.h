#ifndef _GD_DEBUG_H
#define _GD_DEBUG_H

#ifndef GD_ENABLE_TRACE
#define GD_ENABLE_TRACE 1
#endif

#ifndef GD_ENABLE_DEBUG_PRINTF
#define GD_ENABLE_DEBUG_PRINTF 0
#endif

#if GD_ENABLE_DEBUG_PRINTF
#include "stdio.h" 
#include "pico/stdio.h"

#define dprintf printf

#else

#include <stdbool.h>
static inline void __attribute__((format(printf, 1, 2))) dprintf(const char *format, ...) {
        (void)format;
}

static inline bool stdio_init_all(void) {
        return false;
}

#endif

#endif
