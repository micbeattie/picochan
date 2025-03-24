/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_CCW_FETCH_H
#define _PCH_CSS_CCW_FETCH_H

void fetch_chain_data_ccw(pch_schib_t *schib);

uint8_t fetch_first_command_ccw(pch_schib_t *schib);

uint8_t fetch_resume_ccw(pch_schib_t *schib);

uint8_t fetch_chain_ccw(pch_schib_t *schib);

uint8_t fetch_chain_command_ccw(pch_schib_t *schib);

#endif
