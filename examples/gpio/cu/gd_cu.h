#ifndef _GD_CU_H
#define _GD_CU_H

#include "picochan/cu.h"

#ifndef NUM_GPIO_DEVS
#define NUM_GPIO_DEVS 8
#endif

void gd_cu_init(pch_cuaddr_t cua, uint8_t dmairqix);

pch_cu_t *gd_get_cu(void);

void gd_dev_init(pch_devib_t *devib);

#endif
