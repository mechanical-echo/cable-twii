#include "greatest/greatest.h"

SUITE_EXTERN(rng_suite);
SUITE_EXTERN(hash_suite);
SUITE_EXTERN(time_suite);
SUITE_EXTERN(library_suite);
SUITE_EXTERN(config_suite);
SUITE_EXTERN(pool_suite);
SUITE_EXTERN(timed_suite);
SUITE_EXTERN(loop_suite);
SUITE_EXTERN(at_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(rng_suite);
    RUN_SUITE(hash_suite);
    RUN_SUITE(time_suite);
    RUN_SUITE(library_suite);
    RUN_SUITE(config_suite);
    RUN_SUITE(pool_suite);
    RUN_SUITE(timed_suite);
    RUN_SUITE(loop_suite);
    RUN_SUITE(at_suite);
    GREATEST_MAIN_END();
}
