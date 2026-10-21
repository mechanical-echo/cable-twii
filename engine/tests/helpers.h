#ifndef CTW_TEST_HELPERS_H
#define CTW_TEST_HELPERS_H

#include <stdint.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

ctw_db *load_library(const char *extra);
ctw_db *load_config(const char *cfg);
enum greatest_test_res assert_contiguous(const ctw_lineup *l, int64_t from, int64_t to);
int has_warning(const ctw_lineup *l, int channel, ctw_code code, int slot, int item);
size_t count_warnings(const ctw_lineup *l, ctw_code code);

#endif
