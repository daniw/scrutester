/* Host test of input.c's wrapped list index (HW backend, stubbed timer). */
#include <stdio.h>
#include "input.h"

uint16_t stub_tim_cnt;

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void turn(int steps) {
	stub_tim_cnt = (uint16_t) ((stub_tim_cnt + INPUT_ENCODER_RANGE + steps) % INPUT_ENCODER_RANGE);
}

int main(void) {
	input_encoder_wrap_t w;
	static const int ns[] = { 4, 7, 8, 9, 10 };

	for (unsigned k = 0; k < sizeof ns / sizeof ns[0]; k++) {
		int n = ns[k];
		/* several full raw laps each way: index follows +-1 per count */
		stub_tim_cnt = 120;
		input_encoder_wrap_reset(&w, 2);
		int expect = 2;
		for (int i = 0; i < 3 * INPUT_ENCODER_RANGE; i++) {
			turn(1);
			expect = (expect + 1) % n;
			CHECK(input_encoder_read_wrapped(&w, n) == expect);
		}
		for (int i = 0; i < 3 * INPUT_ENCODER_RANGE; i++) {
			turn(-1);
			expect = (expect + n - 1) % n;
			CHECK(input_encoder_read_wrapped(&w, n) == expect);
		}
	}

	/* reset start row, last <-> first, bigger step, n change */
	stub_tim_cnt = 5;
	input_encoder_wrap_reset(&w, 3);
	CHECK(input_encoder_read_wrapped(&w, 4) == 3);
	turn(1);
	CHECK(input_encoder_read_wrapped(&w, 4) == 0);
	turn(-1);
	CHECK(input_encoder_read_wrapped(&w, 4) == 3);
	turn(3);
	CHECK(input_encoder_read_wrapped(&w, 8) == 6);
	input_encoder_reset(77);
	input_encoder_wrap_reset(&w, 5);
	CHECK(input_encoder_read_wrapped(&w, 9) == 5);

	printf(fails ? "FAILED\n" : "OK\n");
	return fails != 0;
}
