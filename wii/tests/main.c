#include "greatest/greatest.h"

SUITE_EXTERN(tween_suite);
SUITE_EXTERN(textfit_suite);
SUITE_EXTERN(clock_suite);
SUITE_EXTERN(tuner_suite);
SUITE_EXTERN(nav_suite);
SUITE_EXTERN(osd_suite);
SUITE_EXTERN(utf8_suite);
SUITE_EXTERN(textcache_suite);
SUITE_EXTERN(volume_suite);
SUITE_EXTERN(playback_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(tween_suite);
    RUN_SUITE(textfit_suite);
    RUN_SUITE(clock_suite);
    RUN_SUITE(tuner_suite);
    RUN_SUITE(nav_suite);
    RUN_SUITE(osd_suite);
    RUN_SUITE(utf8_suite);
    RUN_SUITE(textcache_suite);
    RUN_SUITE(volume_suite);
    RUN_SUITE(playback_suite);
    GREATEST_MAIN_END();
}
