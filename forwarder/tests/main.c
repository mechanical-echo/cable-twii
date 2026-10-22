#include "greatest/greatest.h"

SUITE_EXTERN(dol_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(dol_suite);
    GREATEST_MAIN_END();
}
