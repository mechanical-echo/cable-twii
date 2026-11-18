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

uint32_t pb_play_waiting(pl_state status, uint32_t waiting_ms, uint32_t playing_ms)
{
    if (status != PL_PLAYING) return 0;
    return waiting_ms < playing_ms ? waiting_ms : playing_ms;
}

pb_stall pb_stall_action(uint32_t waiting_ms, bool was_stalled, uint32_t position_ms, uint32_t expected_ms)
{
    if (waiting_ms >= PB_NOTE_MS) return PB_STALL_NOTE;
    if (was_stalled && expected_ms > position_ms && expected_ms - position_ms > PB_REJOIN_LAG_MS) return PB_STALL_REJOIN;
    return PB_STALL_NONE;
}

bool pb_lost(pl_state status, uint32_t waiting_ms)
{
    if (status == PL_ERROR) return true;
    return status == PL_PLAYING && waiting_ms >= PB_GIVEUP_MS;
}

bool pb_probe_due(bool probed, uint32_t since_probe_ms)
{
    return !probed || since_probe_ms >= PB_PROBE_MS;
}
