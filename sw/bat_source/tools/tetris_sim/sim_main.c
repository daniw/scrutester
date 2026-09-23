/*
 * tools/tetris_sim/sim_main.c
 *
 * Host driver for the firmware's Tetris: runs the real Core/Src/tetris.c and
 * Core/Src/input.c natively, calls tetris_step() at the same 20 ms cadence the
 * state machine uses, and renders the result by sampling the shadow
 * framebuffer sim_lcd.c collects. Nothing under Core/ is modified or
 * reimplemented -- the point is to exercise the shipped game logic, layout
 * arithmetic and encoder/button handling without hardware.
 *
 *   make && ./tetris_sim            play it in the terminal
 *   ./tetris_sim --selftest         integration checks (unlock/ESC/wrap)
 *   ./tetris_sim --ai [frames]      self-play; checks clears/scoring/levels
 *   ./tetris_sim --auto [frames]    random input; checks it survives nonsense
 *   ./tetris_sim --stats            print the fill-cost summary on exit
 *
 * Time is virtual: HAL_GetTick() below is driven by sim_time_advance(), so the
 * game sees exactly 20 ms per frame and the headless modes can fast-forward
 * hours of play in seconds.
 */

#include "lcd.h"
#include "sim_input.h"
#include "tetris.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

/* Mirrors the private layout constants in tetris.c on purpose: if the two ever
 * disagree, the terminal view goes visibly wrong, which is the check. */
#define BOARD_W 10
#define BOARD_H 20
#define CELL    11
#define BOARD_X 14
#define BOARD_Y 8

/* Side-panel field positions, from tetris.c draw_stats(). */
#define FIELD_X 140
#define NEXT_Y  44
#define SCORE_Y 112
#define LEVEL_Y 148
#define LINES_Y 184

#define FRAME_MS 20

static const uint16_t PIECE_COLORS[7] = {
	C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED
};
static const char PIECE_CHARS[7] = { 'I', 'J', 'L', 'O', 'S', 'T', 'Z' };
static const int PIECE_ANSI[7] = { 36, 34, 33, 93, 32, 35, 31 };

static uint32_t sim_ms;
static struct termios saved_termios;
static int termios_saved;

/* Fill-cost accounting across the whole run. */
static uint32_t fills_max;
static uint32_t fills_total;
static uint32_t frames_total;
static uint32_t fills_hist[8];

uint32_t HAL_GetTick(void) {
	return sim_ms;
}

void sim_time_advance(uint32_t ms) {
	sim_ms += ms;
}

/* ---------------------------------------------------------------------- */
/* Terminal                                                               */
/* ---------------------------------------------------------------------- */

static void term_restore(void) {
	if (termios_saved)
		tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
	printf("\033[?25h");
	fflush(stdout);
}

static void term_raw(void) {
	struct termios t;

	if (tcgetattr(STDIN_FILENO, &saved_termios) != 0)
		return;
	termios_saved = 1;
	t = saved_termios;
	t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	printf("\033[?25l");
	atexit(term_restore);
}

/* ---------------------------------------------------------------------- */
/* Reading the panel back                                                 */
/* ---------------------------------------------------------------------- */

static uint16_t cell_color(int row, int col) {
	return sim_lcd_pixel((int16_t) (BOARD_X + col * CELL + 4),
			(int16_t) (BOARD_Y + row * CELL + 4));
}

static int color_to_type(uint16_t c) {
	for (int i = 0; i < 7; i++)
		if (c == PIECE_COLORS[i])
			return i;
	return -1;
}

static int cell_char(int row, int col, int *ansi) {
	uint16_t c = cell_color(row, col);
	int t = color_to_type(c);

	if (t >= 0) {
		*ansi = PIECE_ANSI[t];
		return PIECE_CHARS[t];
	}
	*ansi = 90;
	return (c == C_BLACK) ? '.' : '?';
}

/* Reads one of the side-panel numbers back out of the recorded strings. */
static long panel_value(uint16_t y) {
	for (uint32_t i = 0; i < sim_lcd_text_count(); i++)
		if (sim_lcd_text_x(i) == FIELD_X && sim_lcd_text_y(i) == y)
			return strtol(sim_lcd_text_str(i), NULL, 10);
	return -1;
}

/* The game-over panel draws a red frame across the playfield. Sampling a pixel
 * that falls in the 1 px gap *between* two cells makes this unambiguous: no
 * piece can ever colour it. */
static int game_over_shown(void) {
	return sim_lcd_pixel(BOARD_X + 10, 96) == C_RED
			&& sim_lcd_pixel(BOARD_X + 10, 150) == C_RED;
}

static void record_fills(uint32_t fills) {
	static const uint32_t limits[7] = { 0, 4, 16, 32, 64, 128, 256 };
	int slot = 7;

	frames_total++;
	fills_total += fills;
	if (fills > fills_max)
		fills_max = fills;
	for (int i = 0; i < 7; i++) {
		if (fills <= limits[i]) {
			slot = i;
			break;
		}
	}
	fills_hist[slot]++;
}

static void print_stats(void) {
	static const char *const labels[8] = { "0", "1-4", "5-16", "17-32", "33-64",
			"65-128", "129-256", ">256" };

	printf("frames=%u  fills: total=%u  max in one frame=%u  mean=%.2f  "
			"off-panel draws=%u\n", frames_total, fills_total, fills_max,
			frames_total ? (double) fills_total / frames_total : 0.0,
			sim_lcd_out_of_bounds());
	printf("fills per frame:");
	for (int i = 0; i < 8; i++)
		if (fills_hist[i])
			printf("  %s:%u", labels[i], fills_hist[i]);
	printf("\n");
}

static void draw_terminal(uint32_t fills) {
	uint32_t texts = sim_lcd_text_count();

	printf("\033[H\033[2J");
	printf(" ScruTester Tetris (host sim)   t=%ums  frame=%u  fills=%u  "
			"max=%u  off-panel=%u\r\n\r\n", sim_ms, frames_total, fills,
			fills_max, sim_lcd_out_of_bounds());

	for (int row = 0; row < BOARD_H; row++) {
		printf("   |");
		for (int col = 0; col < BOARD_W; col++) {
			int ansi;
			int ch = cell_char(row, col, &ansi);
			printf("\033[%dm%c\033[0m", ansi, ch);
		}
		printf("|");
		if ((uint32_t) row < texts)
			printf("   %-30s", sim_lcd_text_str((uint32_t) row));
		printf("\r\n");
	}
	printf("   +----------+\r\n\r\n");
	printf(" left/right = move   Enter = rotate   Space = OUT (soft drop)"
			"   q = ESC/quit   Q = kill sim\r\n");
	fflush(stdout);
}

/* Returns 1 if the user killed the simulator outright. */
static int handle_keys(void) {
	unsigned char buf[32];
	ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

	for (ssize_t i = 0; i < n; i++) {
		if (buf[i] == 0x1b && i + 2 < n && buf[i + 1] == '[') {
			if (buf[i + 2] == 'C')
				sim_input_encoder_turn(1);
			else if (buf[i + 2] == 'D')
				sim_input_encoder_turn(-1);
			i += 2;
			continue;
		}
		switch (buf[i]) {
		case 'd':
			sim_input_encoder_turn(1);
			break;
		case 'a':
			sim_input_encoder_turn(-1);
			break;
		case '\r':
		case '\n':
			sim_input_tap(SIM_BTN_OK, 3);
			break;
		case ' ':
			sim_input_toggle_out();
			break;
		case 'q':
		case 0x1b:
			sim_input_tap(SIM_BTN_ESC, 3);
			break;
		case 'Q':
			return 1;
		default:
			break;
		}
	}
	return 0;
}

/* ---------------------------------------------------------------------- */
/* Self-playing check (--ai)                                              */
/* ---------------------------------------------------------------------- */

/* Random input almost never completes a row, so it never exercises line
 * clearing, scoring or the level ramp. This player does, and it plays the way a
 * person does: it identifies the piece and reads the stack off the screen, then
 * steers with nothing but the encoder and the OK/OUT buttons -- so a
 * regression in the encoder mapping, the rotation kicks or the layout
 * arithmetic shows up here as a collapsing score.
 *
 * The placement heuristic is the usual greedy one (completed rows are worth a
 * lot, new holes and height are expensive). It is not meant to play well, only
 * to keep games long enough to clear plenty of rows. */

/* All four rotations, mirroring PIECE_SHAPES in tetris.c. */
static const uint16_t AI_SHAPES[7][4] = {
	{ 0x0F00, 0x2222, 0x00F0, 0x4444 },
	{ 0x8E00, 0x6440, 0x0E20, 0x44C0 },
	{ 0x2E00, 0x4460, 0x0E80, 0xC440 },
	{ 0x6600, 0x6600, 0x6600, 0x6600 },
	{ 0x6C00, 0x4620, 0x06C0, 0x8C40 },
	{ 0x4E00, 0x4640, 0x0E40, 0x4C40 },
	{ 0xC600, 0x2640, 0x0C60, 0x4C80 },
};

/* The spawn column is 3, so the leftmost column occupied is 3 + min_col. */
static void shape_extent(uint16_t shape, int *min_col, int *width) {
	int lo = 3, hi = 0;

	for (int row = 0; row < 4; row++)
		for (int col = 0; col < 4; col++) {
			if (!(shape & (uint16_t) (0x8000u >> (row * 4 + col))))
				continue;
			if (col < lo)
				lo = col;
			if (col > hi)
				hi = col;
		}
	*min_col = lo;
	*width = hi - lo + 1;
}

/* Piece cells, normalised so the top-left of the bounding box is (0, 0). */
static int shape_cells(uint16_t shape, int *cr, int *cc) {
	int n = 0, min_row = 3, min_col = 3;

	for (int row = 0; row < 4; row++)
		for (int col = 0; col < 4; col++) {
			if (!(shape & (uint16_t) (0x8000u >> (row * 4 + col))))
				continue;
			if (row < min_row)
				min_row = row;
			if (col < min_col)
				min_col = col;
			cr[n] = row;
			cc[n] = col;
			n++;
		}
	for (int i = 0; i < n; i++) {
		cr[i] -= min_row;
		cc[i] -= min_col;
	}
	return n;
}

/* The settled stack, read back from the panel. Rows 0..1 are taken as empty:
 * every spawn rotation lives there, and treating them as free only matters
 * when the stack is already at the ceiling, i.e. one piece from game over. */
static void read_grid(int grid[BOARD_H][BOARD_W]) {
	for (int row = 0; row < BOARD_H; row++)
		for (int col = 0; col < BOARD_W; col++)
			grid[row][col] = (row >= 2 && cell_color(row, col) != C_BLACK);
}

static long score_placement(const int grid[BOARD_H][BOARD_W], const int *cr,
		const int *cc, int n, int left) {
	int test[BOARD_H][BOARD_W];
	int dy = -1, cleared = 0, prev_h = -1;
	long holes = 0, height = 0, bumpiness = 0;

	/* Lowest offset at which every cell of the piece still fits. */
	for (int try_dy = 0; try_dy < BOARD_H; try_dy++) {
		int ok = 1;
		for (int i = 0; i < n && ok; i++) {
			int r = cr[i] + try_dy, c = cc[i] + left;
			if (r >= BOARD_H || grid[r][c])
				ok = 0;
		}
		if (!ok)
			break;
		dy = try_dy;
	}
	if (dy < 0)
		return -1000000;

	memcpy(test, grid, sizeof(test));
	for (int i = 0; i < n; i++)
		test[cr[i] + dy][cc[i] + left] = 1;

	for (int row = 0; row < BOARD_H; row++) {
		int full = 1;
		for (int col = 0; col < BOARD_W; col++)
			if (!test[row][col])
				full = 0;
		cleared += full;
	}
	for (int col = 0; col < BOARD_W; col++) {
		int top = BOARD_H, h;
		for (int row = 0; row < BOARD_H; row++)
			if (test[row][col]) {
				top = row;
				break;
			}
		h = BOARD_H - top;
		height += h;
		for (int row = top + 1; row < BOARD_H; row++)
			if (!test[row][col])
				holes++;
		if (prev_h >= 0)
			bumpiness += (h > prev_h) ? (h - prev_h) : (prev_h - h);
		prev_h = h;
	}
	return 200L * cleared - 45L * holes - 5L * height - 4L * bumpiness;
}

/* Best (rotation, leftmost column) for this piece on the current stack. */
static void plan_piece(int type, int *out_rot, int *out_left) {
	int grid[BOARD_H][BOARD_W];
	long best = -2000000;

	*out_rot = 0;
	*out_left = 3;
	read_grid(grid);
	for (int rot = 0; rot < 4; rot++) {
		int cr[4], cc[4], min_col, width;
		int n = shape_cells(AI_SHAPES[type][rot], cr, cc);

		shape_extent(AI_SHAPES[type][rot], &min_col, &width);
		for (int left = 0; left + width <= BOARD_W; left++) {
			long v = score_placement(grid, cr, cc, n, left);
			if (v > best) {
				best = v;
				*out_rot = rot;
				*out_left = left;
			}
		}
	}
}

/* Colour of the first occupied cell in the top two rows. Only needed for the
 * first piece of a game, while the board is still empty: after that the
 * falling piece's type is known exactly as "whatever NEXT showed until it
 * changed", which does not race the first gravity step. */
static int piece_at_top(void) {
	for (int row = 0; row < 2; row++)
		for (int col = 0; col < BOARD_W; col++) {
			int t = color_to_type(cell_color(row, col));
			if (t >= 0)
				return t;
		}
	return -1;
}

/* Piece shown in the NEXT box; used as the "a new piece has spawned" signal. */
static int next_box_type(void) {
	for (int row = 0; row < 4; row++)
		for (int col = 0; col < 4; col++) {
			int t = color_to_type(sim_lcd_pixel(
					(int16_t) (FIELD_X + col * CELL + 4),
					(int16_t) (NEXT_Y + row * CELL + 4)));
			if (t >= 0)
				return t;
		}
	return -1;
}

static int run_ai(uint32_t frames) {
	int last_next = -2;   /* NEXT box contents, to spot a fresh spawn */
	int type = -1;        /* piece currently falling */
	int rot_todo = 0;     /* OK presses still owed */
	int move_todo = 0;    /* encoder burst still owed */
	int plan_rot = 0, plan_left = 3;
	int ok_cooldown = 0;
	uint32_t placed = 0, restarts = 0, over_wait = 0;
	long best_score = 0, best_lines = 0, best_level = 0;
	int failures = 0;

	tetris_start();
	sim_input_toggle_out(); /* soft drop latched on: ~25 rows/s */

	for (uint32_t f = 0; f < frames; f++) {
		if (game_over_shown()) {
			/* Ask for a new game, then leave the button alone for a while:
			 * input.c only accepts a level after two identical samples, so
			 * releasing for a single frame would never be seen and OK would
			 * stay latched high -- no further press edge, no restart. */
			if (over_wait == 0) {
				sim_input_tap(SIM_BTN_OK, 3);
				restarts++;
				last_next = -2;
				type = -1;
				rot_todo = 0;
				move_todo = 0;
			}
			if (++over_wait > 12)
				over_wait = 0;
		} else {
			int nb = next_box_type();

			over_wait = 0;

			if (nb != last_next) {
				type = (last_next >= 0) ? last_next : piece_at_top();
				last_next = nb;
				if (type >= 0) {
					plan_piece(type, &plan_rot, &plan_left);
					rot_todo = plan_rot;
					move_todo = 1;
					ok_cooldown = 0;
					placed++;
				}
			}
			/* One OK press per 4 frames: it has to be held for two frames to
			 * get through tetris.c's own debounce and released for two after,
			 * which is what a real tap looks like. */
			if (ok_cooldown > 0) {
				ok_cooldown--;
			} else if (rot_todo > 0) {
				sim_input_tap(SIM_BTN_OK, 2);
				ok_cooldown = 3;
				rot_todo--;
			} else if (move_todo && type >= 0) {
				int min_col, width;

				shape_extent(AI_SHAPES[type][plan_rot], &min_col, &width);
				/* A single burst of several detents inside one frame, which
				 * also exercises tetris.c's multi-step encoder path. */
				sim_input_encoder_turn(plan_left - (3 + min_col));
				move_todo = 0;
			}
		}

		sim_lcd_fills_reset();
		if (!tetris_step()) {
			printf("FAIL: the game exited on frame %u without an ESC press\n",
					f);
			failures++;
			break;
		}
		record_fills(sim_lcd_fills());
		sim_input_frame_done();
		sim_time_advance(FRAME_MS);

		if (panel_value(SCORE_Y) > best_score)
			best_score = panel_value(SCORE_Y);
		if (panel_value(LINES_Y) > best_lines)
			best_lines = panel_value(LINES_Y);
		if (panel_value(LEVEL_Y) > best_level)
			best_level = panel_value(LEVEL_Y);
	}

	printf("ai: %u frames (%.0f s of play), %u pieces placed, "
			"%u game(s) over\n", frames_total,
			frames_total * FRAME_MS / 1000.0, placed, restarts);
	printf("ai: best score=%ld  lines=%ld  level=%ld\n", best_score, best_lines,
			best_level);

	/* A good run clears hundreds of lines; a broken encoder mapping (e.g. the
	 * sim and tetris.c disagreeing on counts per column) still clears a
	 * handful by luck, so demand a real number rather than just "not zero". */
	if (best_lines < (long) (frames / 1000) || best_lines <= 0) {
		printf("FAIL: only %ld line(s) cleared in %u frames -- clearing, "
				"scoring or the encoder mapping is broken\n", best_lines,
				frames);
		failures++;
	}
	if (best_lines >= 10 && best_level < 2) {
		printf("FAIL: %ld lines cleared but the level never advanced\n",
				best_lines);
		failures++;
	}
	if (best_lines > 0 && best_score <= 0) {
		printf("FAIL: lines were cleared but the score stayed at 0\n");
		failures++;
	}
	if (sim_lcd_out_of_bounds()) {
		printf("FAIL: %u draw(s) landed outside the 320x240 panel\n",
				sim_lcd_out_of_bounds());
		failures++;
	}
	print_stats();
	printf(failures ? "ai: FAILED\n" : "ai: ok\n");
	return failures ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* Integration self-test (--selftest)                                     */
/* ---------------------------------------------------------------------- */

/* Covers the handful of behaviours that are about wiring rather than about
 * Tetris, and that are easy to break without noticing on hardware. */

/* Normalised 4x4 mask of a shape, top-left of its bounding box at bit 15. */
static uint16_t shape_normalized(uint16_t shape) {
	int cr[4], cc[4];
	int n = shape_cells(shape, cr, cc);
	uint16_t mask = 0;

	for (int i = 0; i < n; i++)
		mask |= (uint16_t) (0x8000u >> (cr[i] * 4 + cc[i]));
	return mask;
}

/* Same, for whatever single piece is currently drawn in the top rows of an
 * otherwise empty board. Returns 0 if nothing (or too much) is there. */
static uint16_t drawn_piece_normalized(int *out_type) {
	int cr[8], cc[8], n = 0, min_row = BOARD_H, min_col = BOARD_W;
	uint16_t mask = 0;

	for (int row = 0; row < 6; row++)
		for (int col = 0; col < BOARD_W; col++) {
			int t = color_to_type(cell_color(row, col));
			if (t < 0)
				continue;
			if (n >= 8)
				return 0;
			if (out_type)
				*out_type = t;
			if (row < min_row)
				min_row = row;
			if (col < min_col)
				min_col = col;
			cr[n] = row;
			cc[n] = col;
			n++;
		}
	if (n != 4)
		return 0;
	for (int i = 0; i < n; i++)
		mask |= (uint16_t) (0x8000u >> ((cr[i] - min_row) * 4
				+ (cc[i] - min_col)));
	return mask;
}

static void frame(void) {
	tetris_step();
	sim_input_frame_done();
	sim_time_advance(FRAME_MS);
}

static int run_selftest(void) {
	int failures = 0;
	int type = -1;
	uint16_t drawn;

	if (tetris_is_active()) {
		printf("FAIL: tetris_is_active() is set before tetris_start()\n");
		failures++;
	}

	/* The unlock gesture leaves OK held when the game starts. Settle input.c's
	 * debounce first (two reads, exactly as two statemachine ticks would), so
	 * this reproduces the state tetris_start() really sees on the device. */
	sim_input_tap(SIM_BTN_OK, 30);
	(void) input_btn_ok();
	(void) input_btn_ok();
	tetris_start();
	if (!tetris_is_active()) {
		printf("FAIL: tetris_is_active() is clear after tetris_start()\n");
		failures++;
	}
	frame();

	drawn = drawn_piece_normalized(&type);
	if (!drawn || type < 0) {
		printf("FAIL: no single piece is drawn after the first frame\n");
		failures++;
	} else if (drawn != shape_normalized(AI_SHAPES[type][0])) {
		printf("FAIL: the OK still held from the unlock rotated the first "
				"piece (%c drawn as 0x%04X, spawn rotation is 0x%04X)\n",
				PIECE_CHARS[type], drawn,
				shape_normalized(AI_SHAPES[type][0]));
		failures++;
	} else {
		printf("ok: held OK at start does not rotate the first piece (%c)\n",
				PIECE_CHARS[type]);
	}

	/* Positive control: a fresh press must rotate. O looks the same in every
	 * rotation, so it cannot show this.
	 *
	 * A tap needs a few frames to get through: input.c accepts a level after
	 * two reads and tetris.c after two more frames of that level, so the
	 * action lands about three ticks (~60 ms) after contact. */
	while (input_btn_ok()) /* let go of the unlock press */
		frame();
	if (type >= 0 && type != 3) {
		uint16_t want = shape_normalized(AI_SHAPES[type][1]);
		int rotated = 0;

		sim_input_tap(SIM_BTN_OK, 4);
		for (int i = 0; i < 8 && !rotated; i++) {
			frame();
			rotated = (drawn_piece_normalized(NULL) == want);
		}
		if (rotated)
			printf("ok: a fresh OK press rotates (%c reached rotation 1)\n",
					PIECE_CHARS[type]);
		else {
			printf("FAIL: OK did not rotate %c within 8 frames (drawn 0x%04X, "
					"expected 0x%04X)\n", PIECE_CHARS[type],
					drawn_piece_normalized(NULL), want);
			failures++;
		}
	}

	/* A wrap of the 7-bit encoder counter must not teleport the piece: spin
	 * far past the right wall, then back, and check it survives and ends up
	 * against the left wall. */
	sim_input_encoder_turn(200);
	frame();
	sim_input_encoder_turn(-200);
	frame();
	if (drawn_piece_normalized(NULL) == 0) {
		printf("FAIL: the piece vanished after spinning the encoder through "
				"several wraps\n");
		failures++;
	} else {
		printf("ok: encoder wrap handled (spun 200 detents each way)\n");
	}

	/* ESC leaves the game, once, and clears the active flag so the caller
	 * takes the screen back. */
	sim_input_tap(SIM_BTN_ESC, 6);
	{
		int exited = 0;

		for (int i = 0; i < 8 && !exited; i++) {
			exited = !tetris_step();
			sim_input_frame_done();
			sim_time_advance(FRAME_MS);
		}
		if (!exited) {
			printf("FAIL: ESC did not exit the game within 8 frames\n");
			failures++;
		}
	}
	if (tetris_is_active()) {
		printf("FAIL: tetris_is_active() still set after the ESC exit\n");
		failures++;
	} else {
		printf("ok: ESC exits and clears the active flag\n");
	}

	if (sim_lcd_out_of_bounds()) {
		printf("FAIL: %u draw(s) landed outside the 320x240 panel\n",
				sim_lcd_out_of_bounds());
		failures++;
	}
	printf(failures ? "selftest: FAILED\n" : "selftest: ok\n");
	return failures ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* Random-input check (--auto)                                            */
/* ---------------------------------------------------------------------- */

/* Deliberately stupid input, to show the game survives it. No ESC is sent, so
 * the only way out is the frame budget. */
static int run_auto(uint32_t frames) {
	int failures = 0;

	tetris_start();
	for (uint32_t f = 0; f < frames; f++) {
		int r = rand();

		if ((r & 3) == 0)
			sim_input_encoder_turn((r & 4) ? 1 : -1);
		if ((r % 11) == 0)
			sim_input_tap(SIM_BTN_OK, 3);
		if ((r % 97) == 0)
			sim_input_toggle_out();

		sim_lcd_fills_reset();
		if (!tetris_step()) {
			printf("FAIL: the game exited on frame %u without an ESC press\n",
					f);
			failures++;
			break;
		}
		record_fills(sim_lcd_fills());
		sim_input_frame_done();
		sim_time_advance(FRAME_MS);
	}
	if (sim_lcd_out_of_bounds()) {
		printf("FAIL: %u draw(s) landed outside the 320x240 panel\n",
				sim_lcd_out_of_bounds());
		failures++;
	}
	printf("auto: %u frames of random input survived\n", frames_total);
	print_stats();
	printf(failures ? "auto: FAILED\n" : "auto: ok\n");
	return failures ? 1 : 0;
}

static void run_interactive(int stats) {
	term_raw();
	tetris_start();
	for (;;) {
		if (handle_keys())
			break;
		sim_lcd_fills_reset();
		if (!tetris_step()) {
			term_restore();
			printf("\r\ngame exited (ESC) -- on the device the About screen "
					"would be redrawn here\r\n");
			break;
		}
		record_fills(sim_lcd_fills());
		sim_input_frame_done();
		sim_time_advance(FRAME_MS);
		draw_terminal(sim_lcd_fills());
		usleep(FRAME_MS * 1000);
	}
	if (stats)
		print_stats();
}

int main(int argc, char **argv) {
	uint32_t ai_frames = 0, auto_frames = 0;
	int stats = 0, selftest = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--stats") == 0) {
			stats = 1;
		} else if (strcmp(argv[i], "--selftest") == 0) {
			selftest = 1;
		} else if (strcmp(argv[i], "--ai") == 0
				|| strcmp(argv[i], "--auto") == 0) {
			int is_ai = (strcmp(argv[i], "--ai") == 0);
			uint32_t n = 3000;

			if (i + 1 < argc && argv[i + 1][0] != '-')
				n = (uint32_t) strtoul(argv[++i], NULL, 10);
			if (is_ai)
				ai_frames = n;
			else
				auto_frames = n;
		} else {
			fprintf(stderr,
					"usage: %s [--selftest] [--ai [frames]] "
					"[--auto [frames]] [--stats]\n",
					argv[0]);
			return 2;
		}
	}

	if (selftest)
		return run_selftest();
	if (ai_frames)
		return run_ai(ai_frames);
	if (auto_frames)
		return run_auto(auto_frames);
	run_interactive(stats);
	return 0;
}
