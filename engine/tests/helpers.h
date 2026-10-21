#ifndef CTW_TEST_HELPERS_H
#define CTW_TEST_HELPERS_H

#include <stdint.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

ctw_db *load_library(const char *extra);
ctw_db *load_config(const char *cfg);
ctw_db *load_inline(const char *lib, const char *cfg);
int build_lineup(const char *cfg, int channel, ctw_date day, ctw_lineup *l);
enum greatest_test_res assert_contiguous(const ctw_lineup *l, int64_t from, int64_t to);
int has_warning(const ctw_lineup *l, int channel, ctw_code code, int slot, int item);
size_t count_warnings(const ctw_lineup *l, ctw_code code);
enum greatest_test_res assert_capped(const ctw_lineup *l, int channel, int64_t from, int64_t to);

#endif
