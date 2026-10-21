CC ?= cc

ifeq ($(shell uname -s),Darwin)
SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
export SDKROOT
endif

CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -Ivendor
BUILD = build

LIB_SRC = src/rng.c src/hash.c src/timeutil.c src/arena.c src/issues.c src/library.c src/tomlutil.c src/config.c
VENDOR_SRC = vendor/tomlc99/toml.c
VENDOR_OBJ = $(BUILD)/toml.o
VENDOR_CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -Wno-char-subscripts -Ivendor
TEST_SRC = $(wildcard tests/*.c)

.PHONY: test cli clean

test: $(BUILD)/tests
	$(BUILD)/tests

$(BUILD)/toml.o: $(VENDOR_SRC) vendor/tomlc99/toml.h
	@mkdir -p $(BUILD)
	$(CC) $(VENDOR_CFLAGS) -c $(VENDOR_SRC) -o $@

$(BUILD)/tests: $(LIB_SRC) $(VENDOR_OBJ) $(TEST_SRC) $(wildcard src/*.h) include/ctw/ctw.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LIB_SRC) $(VENDOR_OBJ) $(TEST_SRC) -o $@

cli:
	@echo "cli not built yet"

clean:
	rm -rf $(BUILD)
