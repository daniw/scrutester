/*
 * tetris.h
 *
 * Hidden game, reachable by holding OK on the Settings > About screen. It is
 * deliberately self-contained: tetris.c talks to nothing but input.h (encoder
 * + buttons) and the LCD (lcd.h / ugui.h), and never touches the converter,
 * the control loop, the relays or any GPIO. Controls: encoder = move left /
 * right, OK = rotate, OUT = soft drop, ESC = quit.
 *
 * It is driven one frame per statemachine_step() tick (20 ms) rather than
 * from a loop of its own -- see the header comment in tetris.c for why that
 * matters here and what the per-frame drawing budget is.
 */

#ifndef INC_TETRIS_H_
#define INC_TETRIS_H_

#include "stdint.h"

/* How long OK has to be held on the About screen to start the game, in
 * statemachine ticks (20 ms each). statemachine.c already counts held ticks
 * in ok_button_pressed, so the unlock needs no timer or debounce of its own. */
#define TETRIS_UNLOCK_TICKS 75 /* 1.5 s */

/* Takes over the screen: clears it, draws the static chrome and starts a new
 * game. Seeds its own button edge detection from the buttons' current state,
 * so the OK press that unlocked the game is not also read as the first
 * rotate. */
void tetris_start(void);

/* Non-zero while the game owns the LCD and the inputs. */
uint8_t tetris_is_active(void);

/* One frame. Must be called every statemachine tick while the game is active
 * -- both because that is the frame clock and because the encoder helper it
 * uses (input_encoder_read_clamped()) needs to be polled often enough to see
 * the raw counter wrap (see input.h).
 *
 * Returns non-zero while the game is still running, and zero on the tick it
 * exits (ESC), after which the caller owns the screen again and has to redraw
 * it. */
uint8_t tetris_step(void);

#endif /* INC_TETRIS_H_ */
