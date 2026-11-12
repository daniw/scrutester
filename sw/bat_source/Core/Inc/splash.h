/*
 * splash.h
 *
 * Boot splash screen: WHE logo, "Scrutester" title and credit line, drawn
 * with uGUI fill primitives only -- no bitmap is stored in flash. See splash.c.
 */

#ifndef INC_SPLASH_H_
#define INC_SPLASH_H_

/* Clears the screen and draws the splash. Call once, right after LCD_init().
 * It is not removed explicitly: the first display_show_idle() redraws the
 * whole screen, which is what "menu ready" means here (see display_init()). */
void splash_show(void);

#endif /* INC_SPLASH_H_ */
