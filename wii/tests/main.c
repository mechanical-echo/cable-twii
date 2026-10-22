#include "greatest/greatest.h"

SUITE_EXTERN(tween_suite);
SUITE_EXTERN(textfit_suite);
SUITE_EXTERN(clock_suite);
SUITE_EXTERN(tuner_suite);
SUITE_EXTERN(nav_suite);
SUITE_EXTERN(osd_suite);
SUITE_EXTERN(osd_layout_suite);
SUITE_EXTERN(utf8_suite);
SUITE_EXTERN(textcache_suite);
SUITE_EXTERN(volume_suite);
SUITE_EXTERN(playback_suite);
SUITE_EXTERN(aspect_suite);
SUITE_EXTERN(tile_suite);
SUITE_EXTERN(audiobuf_suite);
SUITE_EXTERN(quit_suite);
SUITE_EXTERN(logoanim_suite);
SUITE_EXTERN(guide_model_suite);
SUITE_EXTERN(preview_suite);
SUITE_EXTERN(guide_text_suite);
SUITE_EXTERN(guide_layout_suite);
SUITE_EXTERN(home_layout_suite);
SUITE_EXTERN(state_suite);

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
    RUN_SUITE(osd_layout_suite);
    RUN_SUITE(utf8_suite);
    RUN_SUITE(textcache_suite);
    RUN_SUITE(volume_suite);
    RUN_SUITE(playback_suite);
    RUN_SUITE(aspect_suite);
    RUN_SUITE(tile_suite);
    RUN_SUITE(audiobuf_suite);
    RUN_SUITE(quit_suite);
    RUN_SUITE(logoanim_suite);
    RUN_SUITE(guide_model_suite);
    RUN_SUITE(preview_suite);
    RUN_SUITE(guide_text_suite);
    RUN_SUITE(guide_layout_suite);
    RUN_SUITE(home_layout_suite);
    RUN_SUITE(state_suite);
    GREATEST_MAIN_END();
}
