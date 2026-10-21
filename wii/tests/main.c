#include "greatest/greatest.h"

SUITE_EXTERN(tween_suite);
SUITE_EXTERN(textfit_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(tween_suite);
    RUN_SUITE(textfit_suite);
    GREATEST_MAIN_END();
}
