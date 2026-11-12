/*
 * ui_ctrl.h
 *
 *  Created on: 11.04.2026
 *      Author: Andreas
 */

#ifndef INC_UI_CTRL_H_
#define INC_UI_CTRL_H_

#include "stdint.h"
#include "opt3004.h"
#include "lp581x.h"

extern opt3004_handle hamb;
extern lp581x_handle hled;
extern lp581x_handle hbacklight;

void ui_ctrl_init(void);

void ui_ctrl_Dim(void);
void ui_ctrl_step(void);
uint8_t ui_ctrl_readBacklightPercent(void);

static inline void ui_ctrl_ledSenseOn(void) {
	lp581x_enableChannel(&hled, 2);
}

static inline void ui_ctrl_ledSenseOff(void) {
	lp581x_disableChannel(&hled, 2);
}

static inline void ui_ctrl_ledOutOn(void) {
	lp581x_enableChannel(&hled, 1);
}
static inline void ui_ctrl_ledOutOff(void) {
	lp581x_disableChannel(&hled, 1);
}

static inline uint32_t ui_ctrl_readBrightness(void) {
	return opt3004_readLux(&hamb);
}

#endif /* INC_UI_CTRL_H_ */
