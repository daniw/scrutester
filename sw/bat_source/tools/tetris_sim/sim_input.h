/*
 * tools/tetris_sim/sim_input.h
 *
 * Keyboard -> encoder/button plumbing for the simulator. The real
 * Core/Src/input.c runs on top of this (via stubs/tim.h and stubs/gpio.h), so
 * its debounce and its encoder wrap handling are the ones under test.
 */

#ifndef SIM_INPUT_H
#define SIM_INPUT_H

#include <stdint.h>

#define SIM_BTN_OK  0
#define SIM_BTN_ESC 1
#define SIM_BTN_OUT 2

/* Turns the knob by `detents` (positive = the direction that counts up). */
void sim_input_encoder_turn(int detents);

/* Holds a button for `frames` frames. A terminal gives no key-release event,
 * so a tap is emulated as a short hold; it has to outlast both debounce
 * filters in the chain (input.c's and tetris.c's), hence >= 3 frames. */
void sim_input_tap(int button, int frames);

/* Latches OUT on/off, the way cli.c's keyboard-control mode does for the same
 * reason. */
void sim_input_toggle_out(void);

/* Call once per frame, after tetris_step(), to age the taps. */
void sim_input_frame_done(void);

#endif /* SIM_INPUT_H */
