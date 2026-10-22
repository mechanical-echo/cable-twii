CC ?= cc

ifeq ($(shell uname -s),Darwin)
SDKROOT ?= $(shell xcrun --sdk macosx --show-sdk-path)
export SDKROOT
endif

CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -I../source -I../../engine/vendor
BUILD = build

SRC = ../source/dol.c
TEST_SRC = $(wildcard *.c)

.PHONY: test clean

test: $(BUILD)/tests
	$(BUILD)/tests

$(BUILD)/tests: $(SRC) $(TEST_SRC) $(wildcard ../source/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o $@

clean:
	rm -rf $(BUILD)
