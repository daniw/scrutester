# Host build of the firmware's Tetris (Core/Src/tetris.c), for playing and
# checking the game logic on a PC. Nothing here is part of the firmware build:
# tools/ is outside the CubeIDE source folders and outside tools/check.sh's
# Core/Src/*.c glob.
#
#   make            build ./tetris_sim
#   make play       build and run interactively
#   make check      build and run the headless self-play check
#
# stubs/ must come first on the include path: it shadows Library/LCD/lcd.h,
# Library/UGUI/ugui_fonts.h, Core/Inc/tim.h and Core/Inc/gpio.h so that the
# real tetris.c and input.c compile unmodified against the simulator.

ROOT   := ../..
CC     ?= cc
CFLAGS := -std=gnu11 -O1 -g -Wall -Wextra -Werror -DUGUI_USE_COLOR_RGB565 \
          -Istubs -I$(ROOT)/Core/Inc -I$(ROOT)/Library/UGUI
SRC    := sim_main.c sim_lcd.c sim_input.c \
          $(ROOT)/Core/Src/tetris.c $(ROOT)/Core/Src/input.c

tetris_sim: $(SRC) $(wildcard stubs/*.h) sim_input.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

play: tetris_sim
	./tetris_sim

check: tetris_sim
	./tetris_sim --selftest
	./tetris_sim --ai 40000
	./tetris_sim --auto 6000

clean:
	rm -f tetris_sim

.PHONY: play check clean
