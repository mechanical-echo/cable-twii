#include "playback.h"

pb_end pb_classify_end(bool read_error, bool path_ok)
{
    if (read_error || !path_ok) return PB_END_ERROR;
    return PB_END_EOF;
}

bool pb_needs_probe(pl_state status, uint32_t ms_remaining)
{
    if (status == PL_ERROR) return true;
    return status == PL_ENDED && ms_remaining > PB_ON_TIME_MS;
}

pb_action pb_on_status(pl_state status, uint32_t ms_remaining, bool drive_present)
{
    if (status == PL_ERROR) return drive_present ? PB_BLACK : PB_DISCONNECTED;
    if (status == PL_ENDED && ms_remaining > PB_ON_TIME_MS && !drive_present) return PB_DISCONNECTED;
    return PB_KEEP;
}
