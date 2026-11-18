#ifndef CTW_PLAYBACK_H
#define CTW_PLAYBACK_H

#include <stdbool.h>
#include <stdint.h>
#include "media/player.h"

typedef enum { PB_END_EOF, PB_END_ERROR } pb_end;
typedef enum { PB_KEEP, PB_BLACK, PB_DISCONNECTED } pb_action;
typedef enum { PB_STALL_NONE, PB_STALL_NOTE, PB_STALL_REJOIN } pb_stall;

#define PB_ON_TIME_MS 1000u
#define PB_NOTE_MS 1000u
#define PB_REJOIN_LAG_MS 2000u
#define PB_GIVEUP_MS 10000u
#define PB_PROBE_MS 3000u

pb_end pb_classify_end(bool read_error, bool path_ok);
bool pb_needs_probe(pl_state status, uint32_t ms_remaining);
pb_action pb_on_status(pl_state status, uint32_t ms_remaining, bool drive_present);
uint32_t pb_play_waiting(pl_state status, uint32_t waiting_ms, uint32_t playing_ms);
pb_stall pb_stall_action(uint32_t waiting_ms, bool was_stalled, uint32_t position_ms, uint32_t expected_ms);
bool pb_lost(pl_state status, uint32_t waiting_ms);
bool pb_probe_due(bool probed, uint32_t since_probe_ms);

#endif
