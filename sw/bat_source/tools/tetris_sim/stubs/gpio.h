/*
 * tools/tetris_sim/stubs/gpio.h
 *
 * Host stand-in for Core/Inc/gpio.h, cut down to the three button reads that
 * Core/Src/input.c needs. Backed by sim_input.c, which lets the real,
 * unmodified input.c (including its debounce state machine) run on the PC.
 */

#ifndef SIM_GPIO_H
#define SIM_GPIO_H

#include <stdint.h>

uint8_t gpio_readBtnOk(void);
uint8_t gpio_readBtnEsc(void);
uint8_t gpio_readBtnOut(void);

#endif /* SIM_GPIO_H */
