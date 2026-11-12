#ifndef __FONT_H
#define __FONT_H

#include "ugui.h"

/* Gates Library/LCD/lcd.c's LCD_Test() hardware-bring-up demo (reachable via
 * the CLI's `testLCD` command, Core/Src/cli.c's cmd_testLCD) and this file's
 * "fry" sample bitmap, which together are only needed for manual LCD
 * bring-up, not for production firmware. Off by default to save flash
 * (~17.5KB for fry_data alone); uncomment to re-enable for bring-up/testing,
 * exactly like icon_seed_data.h's ICON_INCLUDE_SEED gates the icon flash
 * seed. */
//#define LCD_TEST_DEMO_ENABLED

extern UG_BMP fry;

#endif
