/* Host test of stopwatch.c. */
#include <stdio.h>
#include "stopwatch.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
	/* manual */
	stopwatch_toggle(1000);
	CHECK(stopwatch_running() && stopwatch_busy());
	CHECK(stopwatch_elapsed_ms(1500) == 500);
	stopwatch_toggle(2000);
	CHECK(!stopwatch_running() && stopwatch_elapsed_ms(9000) == 1000);
	stopwatch_toggle(5000);
	stopwatch_toggle(5250);
	CHECK(stopwatch_elapsed_ms(9000) == 1250);
	stopwatch_clear();
	CHECK(stopwatch_elapsed_ms(9000) == 0 && !stopwatch_busy());

	/* IMD: refused at or below the stop level */
	CHECK(!stopwatch_vt_arm(VT_KIND_IMD, 60000, 59000, 100));
	CHECK(stopwatch_vt_state() == VT_IDLE);

	/* IMD: runs at once, stops below the level */
	CHECK(stopwatch_vt_arm(VT_KIND_IMD, 60000, 571200, 100));
	CHECK(stopwatch_vt_state() == VT_RUNNING && stopwatch_busy());
	stopwatch_sample(300000, 200);
	CHECK(stopwatch_vt_state() == VT_RUNNING);
	CHECK(stopwatch_vt_elapsed_ms(300) == 200);
	stopwatch_sample(59800, 4935);
	CHECK(stopwatch_vt_state() == VT_DONE && !stopwatch_busy());
	CHECK(stopwatch_vt_elapsed_ms(99999) == 4835);
	CHECK(stopwatch_vt_start_mV() == 571200 && stopwatch_vt_stop_mV() == 59800);
	CHECK(stopwatch_vt_take_done() == 1 && stopwatch_vt_take_done() == 0);

	/* Discharge: refused at or below the stop level */
	CHECK(!stopwatch_vt_arm(VT_KIND_DISCHARGE, 60000, 60000, 0));
	CHECK(stopwatch_vt_state() == VT_DONE); /* unchanged by the refusal */

	/* Discharge: arm at 500 V, start below 475 V */
	CHECK(stopwatch_vt_arm(VT_KIND_DISCHARGE, 60000, 500000, 0));
	CHECK(stopwatch_vt_state() == VT_ARMED && stopwatch_busy());
	stopwatch_sample(480000, 10);
	CHECK(stopwatch_vt_state() == VT_ARMED);
	stopwatch_sample(474000, 20);
	CHECK(stopwatch_vt_state() == VT_RUNNING && stopwatch_vt_start_mV() == 474000);
	stopwatch_sample(100000, 520);
	stopwatch_sample(59000, 1020);
	CHECK(stopwatch_vt_state() == VT_DONE && stopwatch_vt_elapsed_ms(0) == 1000);

	/* Cancel and reset */
	stopwatch_vt_arm(VT_KIND_DISCHARGE, 60000, 500000, 0);
	stopwatch_vt_cancel();
	CHECK(stopwatch_vt_state() == VT_IDLE);
	stopwatch_vt_arm(VT_KIND_IMD, 60000, 500000, 0);
	stopwatch_reset();
	CHECK(!stopwatch_busy());

	/* tick wrap */
	stopwatch_toggle(0xFFFFFF00u);
	CHECK(stopwatch_elapsed_ms(0x100u) == 0x200u);

	printf(fails ? "stopwatch: %d FAILED\n" : "stopwatch: all passed\n", fails);
	return fails != 0;
}
