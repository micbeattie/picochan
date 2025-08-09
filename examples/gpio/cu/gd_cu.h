#ifndef _GD_CU_H
#define _GD_CU_H

#include "picochan/cu.h"

#ifndef NUM_GPIO_DEVS
#define NUM_GPIO_DEVS 8
#endif

void gd_cu_init(pch_cuaddr_t cua);

#endif
