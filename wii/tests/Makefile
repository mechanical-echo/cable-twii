CC ?= cc

ifeq ($(shell uname -s),Darwin)
SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
export SDKROOT
endif

CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../source -I../../engine/vendor -I../../engine/include
BUILD = build

ENGINE_SRC = ../../engine/src/rng.c ../../engine/src/hash.c ../../engine/src/timeutil.c ../../engine/src/arena.c \
             ../../engine/src/issues.c ../../engine/src/library.c ../../engine/src/tomlutil.c ../../engine/src/config.c \
             ../../engine/src/pool.c ../../engine/src/lineup.c ../../engine/src/lineup_place.c ../../engine/src/lineup_timed.c \
             ../../engine/src/lineup_loop.c ../../engine/src/db.c
VENDOR_SRC = ../../engine/vendor/tomlc99/toml.c
VENDOR_OBJ = $(BUILD)/toml.o
VENDOR_CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -Wno-char-subscripts -I../../engine/vendor

SRC = ../source/ui/tween.c ../source/ui/textfit.c ../source/ui/utf8.c ../source/ui/textcache.c ../source/clock.c ../source/tuner.c ../source/nav.c ../source/osd.c ../source/osd_layout.c ../source/volume.c ../source/playback.c ../source/aspect.c ../source/media/tile.c ../source/media/audiobuf.c ../source/quit.c ../source/logoanim.c ../source/guide_model.c ../source/preview.c ../source/guide_text.c ../source/guide_layout.c ../source/home_layout.c ../source/state.c ../source/debug/dbgstats.c $(ENGINE_SRC)
TEST_SRC = $(wildcard *.c)

.PHONY: test clean

test: $(BUILD)/tests
	@mkdir -p $(BUILD)/state
	$(BUILD)/tests

$(VENDOR_OBJ): $(VENDOR_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(VENDOR_CFLAGS) -c $(VENDOR_SRC) -o $@

$(BUILD)/tests: $(SRC) $(VENDOR_OBJ) $(TEST_SRC) $(wildcard ../source/ui/*.h) $(wildcard ../source/*.h) $(wildcard ../source/media/*.h) $(wildcard ../source/debug/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) $(VENDOR_OBJ) $(TEST_SRC) -o $@

clean:
	rm -rf $(BUILD)
