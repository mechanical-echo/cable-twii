#include "playback.h"

pb_end pb_classify_end(bool read_error, bool path_ok)
{
    if (read_error || !path_ok) return PB_END_ERROR;
    return PB_END_EOF;
}

pb_action pb_on_status(pl_state status, bool drive_present)
{
    if (status != PL_ERROR) return PB_KEEP;
    return drive_present ? PB_BLACK : PB_DISCONNECTED;
}
