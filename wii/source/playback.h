#ifndef CTW_PLAYBACK_H
#define CTW_PLAYBACK_H

#include <stdbool.h>
#include <stdint.h>
#include "media/player.h"

typedef enum { PB_END_EOF, PB_END_ERROR } pb_end;
typedef enum { PB_KEEP, PB_BLACK, PB_DISCONNECTED } pb_action;

#define PB_ON_TIME_MS 1000u

pb_end pb_classify_end(bool read_error, bool path_ok);
bool pb_needs_probe(pl_state status, uint32_t ms_remaining);
pb_action pb_on_status(pl_state status, uint32_t ms_remaining, bool drive_present);

#endif
