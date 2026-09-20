/*
 * tools/tetris_sim/stubs/tim.h
 *
 * Host stand-in for Core/Inc/tim.h, cut down to the quadrature-encoder
 * accessors Core/Src/input.c needs. sim_input.c emulates TIM4's 7-bit
 * wrapping counter (ARR = 127), so the wrap handling in
 * input_encoder_read_clamped() is exercised for real here.
 */

#ifndef SIM_TIM_H
#define SIM_TIM_H

#include <stdint.h>

uint16_t tim_encoder_read(void);
void tim_encoder_reset(uint8_t value);

#endif /* SIM_TIM_H */
