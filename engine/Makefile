CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -Ivendor
BUILD = build

LIB_SRC = src/rng.c src/hash.c src/timeutil.c
VENDOR_SRC =
TEST_SRC = $(wildcard tests/*.c)

.PHONY: test cli clean

test: $(BUILD)/tests
	$(BUILD)/tests

$(BUILD)/tests: $(LIB_SRC) $(VENDOR_SRC) $(TEST_SRC) include/ctw/ctw.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LIB_SRC) $(VENDOR_SRC) $(TEST_SRC) -o $@

cli:
	@echo "cli not built yet"

clean:
	rm -rf $(BUILD)
