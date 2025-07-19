#ifndef _GD_DEBUG_H
#define _GD_DEBUG_H

#if GD_ENABLE_GPIO_VERBOSE 
#include "stdio.h" 
#define dprintf printf
#else
static inline void __attribute__((format(printf, 1, 2))) dprintf(const char *format, ...) {}
#endif

#endif
