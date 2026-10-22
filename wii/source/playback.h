#ifndef CTW_PLAYBACK_H
#define CTW_PLAYBACK_H

#include <stdbool.h>
#include "media/player.h"

typedef enum { PB_END_EOF, PB_END_ERROR } pb_end;
typedef enum { PB_KEEP, PB_BLACK, PB_DISCONNECTED } pb_action;

pb_end pb_classify_end(bool read_error, bool path_ok);
pb_action pb_on_status(pl_state status, bool drive_present);

#endif
