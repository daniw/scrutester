/*
 * tetris.c
 *
 * See tetris.h. Notes on the two things that shape this file:
 *
 * 1. No loop of its own. The firmware is single-threaded and event-driven
 *    (main.c drains one event per ~1 ms iteration), so a game loop here would
 *    stall cli_loop(), the event queue, the 1 s BMS poll, the backlight fade
 *    in ui_ctrl_step() and protection_update(). Instead tetris_step() does
 *    exactly one frame and returns, called from the existing 20 ms
 *    statemachine tick.
 *
 * 2. Only changed cells are drawn. There is no local framebuffer
 *    (lcd.h LCD_LOCAL_FB is off), so every draw is a synchronous SPI write
 *    and LCD_WriteData() spin-waits on the DMA: a full screen is ~76800
 *    pixels, i.e. ~31 ms at 40 Mbit/s, which does not fit in a 20 ms tick.
 *    So the static chrome is drawn once in tetris_start() (same enter/update
 *    split as display.c), and each frame composes board + falling piece into
 *    `comp`, then fills only the cells where `comp` differs from what the
 *    panel already shows (`shown`). A quiet frame is 2..8 cells (~0.3 ms);
 *    the worst case is a line clear shifting the whole stack, which can reach
 *    ~200 cells (~15 ms) in one frame. Overrunning a tick is harmless anyway:
 *    EVENT_SM_STEP is coalesced in event.c, so a late frame drops the next
 *    tick instead of queueing it -- which is also why gravity is timed off
 *    HAL_GetTick() below and not by counting frames.
 */

#include "tetris.h"
#include "lcd.h"
#include "ugui_fonts.h"
#include "input.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* Layout (landscape 320 x 240, see lcd.h LCD_ROTATION)                    */
/* ---------------------------------------------------------------------- */

#define BOARD_W       10
#define BOARD_H       20
#define CELL          11              /* cell pitch; the block itself is CELL-1,
                                       * leaving a 1 px gap that stays at the
                                       * background colour and draws the grid
                                       * for free */
#define BOARD_X       14
#define BOARD_Y       8
#define BOARD_W_PX    (BOARD_W * CELL)
#define BOARD_H_PX    (BOARD_H * CELL)

#define PANEL_X       140
#define NEXT_BOX      (4 * CELL)
#define NEXT_Y        44

#define FONT_SMALL    FONT_12X16      /* both fonts are already referenced by */
#define FONT_TINY     FONT_8X12       /* display.c, so they cost no extra flash */

#define COL_BG        C_BLACK
#define COL_FRAME     C_DIM_GRAY
#define COL_LABEL     C_WHITE_63
#define COL_VALUE     C_WHITE

/* ---------------------------------------------------------------------- */
/* Rules                                                                   */
/* ---------------------------------------------------------------------- */

#define FALL_MS_BASE     500          /* level 1 */
#define FALL_MS_PER_LVL  40
#define FALL_MS_MIN      100
#define SOFT_DROP_MS     40           /* while OUT is held */
#define LINES_PER_LEVEL  10

/* Raw encoder counts per mechanical detent (see input.h) = one column. */
#define ENC_PER_STEP     1
/* Set to 1 if turning clockwise should move the piece left instead of right.
 * Which way the encoder counts up can only be confirmed on the bench. */
#define ENC_INVERT       0
/* Bounds for the free-running encoder position. input_encoder_read_clamped()
 * is used purely for its wrap-aware delta here, so these are picked far
 * outside anything a human can turn to in one session -- the clamp never
 * engages, the position never overflows. */
#define ENC_MIN          (-1000000)
#define ENC_MAX          (1000000)
/* A sanity bound on how many columns one frame may move the piece, so a
 * violently spun knob cannot turn into a long loop inside one tick. */
#define ENC_MAX_STEPS    8

/* Each rotation is a 4x4 bitmap, most significant bit = top-left, read
 * row by row -- so every nibble below is one row of the piece as it looks on
 * screen. Rotation is clockwise. */
static const uint16_t PIECE_SHAPES[7][4] = {
	{ 0x0F00, 0x2222, 0x00F0, 0x4444 },  /* I */
	{ 0x8E00, 0x6440, 0x0E20, 0x44C0 },  /* J */
	{ 0x2E00, 0x4460, 0x0E80, 0xC440 },  /* L */
	{ 0x6600, 0x6600, 0x6600, 0x6600 },  /* O */
	{ 0x6C00, 0x4620, 0x06C0, 0x8C40 },  /* S */
	{ 0x4E00, 0x4640, 0x0E40, 0x4C40 },  /* T */
	{ 0xC600, 0x2640, 0x0C60, 0x4C80 },  /* Z */
};

/* Indexed by piece type; board cells store type + 1 so that 0 means empty. */
static const uint16_t PIECE_COLORS[7] = {
	C_CYAN, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED
};

/* Cleared lines -> score, multiplied by the level. */
static const uint16_t LINE_SCORE[5] = { 0, 40, 100, 300, 1200 };

#define SHAPE_CELL(shape, row, col) \
	((shape) & (uint16_t) (0x8000u >> ((row) * 4 + (col))))

/* ---------------------------------------------------------------------- */
/* State                                                                   */
/* ---------------------------------------------------------------------- */

/* Debounced level plus press-edge detection, private to the game.
 *
 * input.c already debounces, but it does so per *call*, and while the game
 * runs the buttons are read twice per tick (statemachine_step()'s own
 * prologue reads them first) -- two identical reads a few microseconds apart
 * satisfy its BTN_DEBOUNCE_SAMPLES on their own, which shortens its filter to
 * effectively one sample. Requiring two consecutive *frames* of the same
 * level here restores a 40 ms filter for the game's own reads without
 * touching the shared input layer. */
typedef struct {
	uint8_t last;   /* level seen on the previous frame */
	uint8_t stable; /* accepted level */
} button_t;

static uint8_t active;
static uint8_t game_over;

static uint8_t board[BOARD_H][BOARD_W]; /* locked cells: 0 = empty, else type+1 */
static uint8_t comp[BOARD_H][BOARD_W];  /* board + falling piece, rebuilt per frame */
static uint8_t shown[BOARD_H][BOARD_W]; /* what the panel currently displays */

static uint8_t piece_type;
static uint8_t piece_rot;
static int8_t piece_x;
static int8_t piece_y;
static uint8_t next_type;

static uint8_t bag[7];
static uint8_t bag_left;
static uint32_t rng_state;

static uint32_t score;
static uint16_t lines_total;
static uint16_t level;
static uint32_t last_fall_ms;

static button_t btn_ok;
static button_t btn_esc;
static button_t btn_out;
static input_encoder_clamp_t enc;
static int32_t enc_applied;           /* encoder position already turned into moves */

/* Last values painted into the side panel, so text is only redrawn when it
 * actually changes (LCD_PutStr is per-glyph and comparatively expensive). */
static uint32_t shown_score;
static uint16_t shown_lines;
static uint16_t shown_level;
static uint8_t shown_next;

/* ---------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ---------------------------------------------------------------------- */

/* xorshift32. rand() is deliberately avoided: nothing in this firmware pulls
 * in newlib's RNG today, and this is 3 lines and reproducible. */
static uint32_t rng_next(void) {
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/* Refills the 7-bag and shuffles it, so every piece appears once per bag
 * instead of the same one repeating six times by chance. */
static void bag_refill(void) {
	for (uint8_t i = 0; i < 7; i++)
		bag[i] = i;
	for (uint8_t i = 6; i > 0; i--) {
		uint8_t j = (uint8_t) (rng_next() % (uint32_t) (i + 1));
		uint8_t t = bag[i];
		bag[i] = bag[j];
		bag[j] = t;
	}
	bag_left = 7;
}

static uint8_t bag_take(void) {
	if (bag_left == 0)
		bag_refill();
	return bag[--bag_left];
}

/* Non-zero if the piece would sit legally at (px, py) in that rotation.
 * Cells above the top edge are ignored rather than rejected, so a piece whose
 * top bitmap row is empty can still spawn at row 0. */
static uint8_t piece_fits(uint8_t type, uint8_t rot, int8_t px, int8_t py) {
	uint16_t shape = PIECE_SHAPES[type][rot];

	for (uint8_t row = 0; row < 4; row++) {
		for (uint8_t col = 0; col < 4; col++) {
			int16_t bx, by;
			if (!SHAPE_CELL(shape, row, col))
				continue;
			bx = (int16_t) (px + (int8_t) col);
			by = (int16_t) (py + (int8_t) row);
			if (bx < 0 || bx >= BOARD_W || by >= BOARD_H)
				return 0;
			if (by >= 0 && board[by][bx])
				return 0;
		}
	}
	return 1;
}

static uint8_t try_move(int8_t dx) {
	if (!piece_fits(piece_type, piece_rot, (int8_t) (piece_x + dx), piece_y))
		return 0;
	piece_x = (int8_t) (piece_x + dx);
	return 1;
}

/* Rotates clockwise, nudging sideways by up to two columns if the rotation
 * would otherwise be blocked (a "wall kick" -- without it, pieces cannot be
 * turned while resting against a wall or the stack, which feels broken). */
static void try_rotate(void) {
	static const int8_t KICKS[5] = { 0, -1, 1, -2, 2 };
	uint8_t rot = (uint8_t) ((piece_rot + 1) & 3);

	for (uint8_t i = 0; i < 5; i++) {
		if (piece_fits(piece_type, rot, (int8_t) (piece_x + KICKS[i]), piece_y)) {
			piece_x = (int8_t) (piece_x + KICKS[i]);
			piece_rot = rot;
			return;
		}
	}
}

static uint16_t fall_interval_ms(void) {
	int32_t ms = FALL_MS_BASE - FALL_MS_PER_LVL * (int32_t) (level - 1);
	if (ms < FALL_MS_MIN)
		ms = FALL_MS_MIN;
	return (uint16_t) ms;
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/* ---------------------------------------------------------------------- */

static void draw_cell(uint8_t row, uint8_t col, uint8_t value) {
	int16_t x = (int16_t) (BOARD_X + col * CELL);
	int16_t y = (int16_t) (BOARD_Y + row * CELL);
	UG_FillFrame(x, y, (int16_t) (x + CELL - 2), (int16_t) (y + CELL - 2),
			value ? PIECE_COLORS[value - 1] : COL_BG);
}

/* Board + falling piece -> comp, then one fill per cell that actually
 * changed. This is the whole per-frame drawing cost in the steady state. */
static void render_board(void) {
	memcpy(comp, board, sizeof(comp));

	if (!game_over) {
		uint16_t shape = PIECE_SHAPES[piece_type][piece_rot];
		for (uint8_t row = 0; row < 4; row++) {
			for (uint8_t col = 0; col < 4; col++) {
				int16_t bx = (int16_t) (piece_x + (int8_t) col);
				int16_t by = (int16_t) (piece_y + (int8_t) row);
				if (!SHAPE_CELL(shape, row, col))
					continue;
				if (by < 0 || by >= BOARD_H || bx < 0 || bx >= BOARD_W)
					continue;
				comp[by][bx] = (uint8_t) (piece_type + 1);
			}
		}
	}

	for (uint8_t row = 0; row < BOARD_H; row++) {
		for (uint8_t col = 0; col < BOARD_W; col++) {
			if (comp[row][col] == shown[row][col])
				continue;
			draw_cell(row, col, comp[row][col]);
			shown[row][col] = comp[row][col];
		}
	}
}

static void draw_next(void) {
	uint16_t shape = PIECE_SHAPES[next_type][0];

	UG_FillFrame(PANEL_X, NEXT_Y, (int16_t) (PANEL_X + NEXT_BOX - 1),
			(int16_t) (NEXT_Y + NEXT_BOX - 1), COL_BG);
	for (uint8_t row = 0; row < 4; row++) {
		for (uint8_t col = 0; col < 4; col++) {
			int16_t x = (int16_t) (PANEL_X + col * CELL);
			int16_t y = (int16_t) (NEXT_Y + row * CELL);
			if (!SHAPE_CELL(shape, row, col))
				continue;
			UG_FillFrame(x, y, (int16_t) (x + CELL - 2), (int16_t) (y + CELL - 2),
					PIECE_COLORS[next_type]);
		}
	}
	shown_next = next_type;
}

/* Fixed-width formatting so a shorter value overwrites a longer one without a
 * separate erase -- same idiom as display.c. */
static void draw_stats(void) {
	char text[12];

	if (score != shown_score) {
		snprintf(text, sizeof(text), "%6lu", (unsigned long) score);
		LCD_PutStr(PANEL_X, 112, text, FONT_SMALL, COL_VALUE, COL_BG);
		shown_score = score;
	}
	if (level != shown_level) {
		snprintf(text, sizeof(text), "%3u", (unsigned) level);
		LCD_PutStr(PANEL_X, 148, text, FONT_SMALL, COL_VALUE, COL_BG);
		shown_level = level;
	}
	if (lines_total != shown_lines) {
		snprintf(text, sizeof(text), "%3u", (unsigned) lines_total);
		LCD_PutStr(PANEL_X, 184, text, FONT_SMALL, COL_VALUE, COL_BG);
		shown_lines = lines_total;
	}
	if (next_type != shown_next)
		draw_next();
}

static void draw_chrome(void) {
	UG_FillScreen(COL_BG);

	/* 2 px playfield border, just outside the cell grid. */
	UG_DrawFrame((int16_t) (BOARD_X - 2), (int16_t) (BOARD_Y - 2),
			(int16_t) (BOARD_X + BOARD_W_PX), (int16_t) (BOARD_Y + BOARD_H_PX),
			COL_FRAME);
	UG_DrawFrame((int16_t) (BOARD_X - 3), (int16_t) (BOARD_Y - 3),
			(int16_t) (BOARD_X + BOARD_W_PX + 1),
			(int16_t) (BOARD_Y + BOARD_H_PX + 1), COL_FRAME);

	LCD_PutStr(PANEL_X, 6, "ScruTetris", FONT_SMALL, C_DODGER_BLUE, COL_BG);
	LCD_PutStr(PANEL_X, 30, "NEXT", FONT_TINY, COL_LABEL, COL_BG);
	LCD_PutStr(PANEL_X, 98, "SCORE", FONT_TINY, COL_LABEL, COL_BG);
	LCD_PutStr(PANEL_X, 134, "LEVEL", FONT_TINY, COL_LABEL, COL_BG);
	LCD_PutStr(PANEL_X, 170, "LINES", FONT_TINY, COL_LABEL, COL_BG);
	LCD_PutStr(PANEL_X, 212, "OUT: drop", FONT_TINY, COL_LABEL, COL_BG);
	LCD_PutStr(PANEL_X, 226, "ESC: exit", FONT_TINY, COL_LABEL, COL_BG);
}

static void draw_game_over(void) {
	const int16_t x0 = BOARD_X, x1 = (int16_t) (BOARD_X + BOARD_W_PX - 1);
	const int16_t y0 = 96, y1 = 150;

	UG_FillFrame(x0, y0, x1, y1, COL_BG);
	UG_DrawFrame(x0, y0, x1, y1, C_RED);
	/* 9 and 9 characters at 8 px, centred in the 110 px playfield by hand. */
	LCD_PutStr((uint16_t) (x0 + 19), 108, "GAME OVER", FONT_TINY, C_RED, COL_BG);
	LCD_PutStr((uint16_t) (x0 + 19), 128, "OK: again", FONT_TINY, COL_LABEL,
			COL_BG);
}

/* ---------------------------------------------------------------------- */
/* Game flow                                                               */
/* ---------------------------------------------------------------------- */

static void spawn_piece(void) {
	piece_type = next_type;
	next_type = bag_take();
	piece_rot = 0;
	piece_x = 3;
	piece_y = 0;
	if (!piece_fits(piece_type, piece_rot, piece_x, piece_y))
		game_over = 1;
}

static uint8_t clear_full_lines(void) {
	uint8_t cleared = 0;
	int8_t row = BOARD_H - 1;

	while (row >= 0) {
		uint8_t full = 1;
		for (uint8_t col = 0; col < BOARD_W; col++) {
			if (!board[row][col]) {
				full = 0;
				break;
			}
		}
		if (!full) {
			row--;
			continue;
		}
		/* Pull everything above down by one and keep re-testing this same row,
		 * which now holds what used to be above it. */
		for (int8_t r = row; r > 0; r--)
			memcpy(board[r], board[r - 1], BOARD_W);
		memset(board[0], 0, BOARD_W);
		cleared++;
	}
	return cleared;
}

static void lock_piece(void) {
	uint16_t shape = PIECE_SHAPES[piece_type][piece_rot];
	uint8_t cleared;

	for (uint8_t row = 0; row < 4; row++) {
		for (uint8_t col = 0; col < 4; col++) {
			int16_t bx = (int16_t) (piece_x + (int8_t) col);
			int16_t by = (int16_t) (piece_y + (int8_t) row);
			if (!SHAPE_CELL(shape, row, col))
				continue;
			if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
				board[by][bx] = (uint8_t) (piece_type + 1);
		}
	}

	cleared = clear_full_lines();
	if (cleared > 4)
		cleared = 4; /* only four rows can ever complete at once; keeps the
		              * LINE_SCORE index provably in range */
	if (cleared) {
		score += (uint32_t) LINE_SCORE[cleared] * level;
		lines_total = (uint16_t) (lines_total + cleared);
		level = (uint16_t) (1 + lines_total / LINES_PER_LEVEL);
	}
	spawn_piece();
}

static void step_down(void) {
	if (piece_fits(piece_type, piece_rot, piece_x, (int8_t) (piece_y + 1)))
		piece_y = (int8_t) (piece_y + 1);
	else
		lock_piece();
}

/* Starts a new game on an already-drawn screen. The explicit fill of the
 * playfield is needed because `shown` is invalidated below: the 1 px gaps
 * between cells are never painted by draw_cell(), so anything drawn across
 * the playfield (the game-over panel) has to be erased in one go. */
static void reset_game(void) {
	UG_FillFrame(BOARD_X, BOARD_Y, (int16_t) (BOARD_X + BOARD_W_PX - 1),
			(int16_t) (BOARD_Y + BOARD_H_PX - 1), COL_BG);
	memset(board, 0, sizeof(board));
	memset(shown, 0xFF, sizeof(shown)); /* no valid cell value -> repaint all */

	game_over = 0;
	score = 0;
	lines_total = 0;
	level = 1;
	shown_score = 0xFFFFFFFFu;
	shown_level = 0xFFFFu;
	shown_lines = 0xFFFFu;
	shown_next = 0xFF;

	bag_left = 0;
	next_type = bag_take();
	spawn_piece();
	last_fall_ms = HAL_GetTick();
}

/* ---------------------------------------------------------------------- */
/* Input                                                                   */
/* ---------------------------------------------------------------------- */

static void button_init(button_t *b, uint8_t level_now) {
	b->last = level_now;
	b->stable = level_now;
}

/* Returns non-zero on the frame the button goes from released to pressed. */
static uint8_t button_edge(button_t *b, uint8_t raw) {
	uint8_t before = b->stable;

	if (raw == b->last)
		b->stable = raw;
	b->last = raw;
	return (uint8_t) (b->stable && !before);
}

/* Turns encoder rotation since the last frame into single-column moves, each
 * collision-checked on its own so a fast spin cannot slide a piece through
 * the wall or the stack. input_encoder_read_clamped() is used (rather than
 * the raw count) for its wrap handling: the hardware counter rolls over at
 * 0/127, and its bounds here are far outside reach, so it acts as a
 * free-running signed position. */
static void apply_encoder(void) {
	int32_t pos = input_encoder_read_clamped(&enc, ENC_MIN, ENC_MAX);
	int8_t dir = ENC_INVERT ? -1 : 1;
	uint8_t steps = 0;

	while (pos - enc_applied >= ENC_PER_STEP && steps < ENC_MAX_STEPS) {
		try_move(dir);
		enc_applied += ENC_PER_STEP;
		steps++;
	}
	while (enc_applied - pos >= ENC_PER_STEP && steps < ENC_MAX_STEPS) {
		try_move((int8_t) -dir);
		enc_applied -= ENC_PER_STEP;
		steps++;
	}
	/* Drop any leftover beyond the step budget, so the piece cannot keep
	 * gliding for frames after the knob stopped. */
	if (steps >= ENC_MAX_STEPS)
		enc_applied = pos;
}

/* ---------------------------------------------------------------------- */
/* Public interface                                                        */
/* ---------------------------------------------------------------------- */

void tetris_start(void) {
	/* Seeded from the tick the player happened to unlock the game on, mixed
	 * with the encoder position, so the piece sequence differs between runs.
	 * Must never be 0 (xorshift's fixed point). */
	rng_state = HAL_GetTick() ^ ((uint32_t) input_encoder_read() << 16)
			^ 0xA5A5A5A5u;
	if (rng_state == 0)
		rng_state = 1;

	/* OK is still held from the unlock gesture, and ESC/OUT may be held too:
	 * seeding from the current levels means none of them is seen as a fresh
	 * press on the first frames. */
	button_init(&btn_ok, input_btn_ok());
	button_init(&btn_esc, input_btn_esc());
	button_init(&btn_out, input_btn_out());

	/* The game's own frame of reference for the encoder. The raw hardware
	 * counter is left alone on purpose -- the idle carousel and the settings
	 * list navigate by it, and input_encoder_reset() would move it under
	 * them. */
	input_encoder_clamp_reset(&enc, 0);
	enc_applied = 0;

	draw_chrome();
	reset_game();
	active = 1;
}

uint8_t tetris_is_active(void) {
	return active;
}

uint8_t tetris_step(void) {
	uint8_t ok_edge = button_edge(&btn_ok, input_btn_ok());
	uint8_t esc_edge = button_edge(&btn_esc, input_btn_esc());
	uint32_t now;
	uint16_t interval;

	(void) button_edge(&btn_out, input_btn_out()); /* OUT is used as a level */

	if (esc_edge) {
		active = 0;
		return 0;
	}

	if (game_over) {
		if (ok_edge)
			reset_game();
		/* Keep the encoder frame of reference following the knob while the
		 * game is over, so turning it during the pause does not move the
		 * first piece of the next game. */
		enc_applied = input_encoder_read_clamped(&enc, ENC_MIN, ENC_MAX);
		return 1;
	}

	apply_encoder();
	if (ok_edge)
		try_rotate();

	now = HAL_GetTick();
	interval = btn_out.stable ? SOFT_DROP_MS : fall_interval_ms();
	if (now - last_fall_ms >= interval) {
		last_fall_ms = now;
		step_down();
	}

	render_board();
	draw_stats();
	if (game_over)
		draw_game_over();

	return 1;
}
