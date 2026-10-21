#include "lineup_internal.h"

int ctw_lineup_build_timed(ctw_build *b)
{
    const ctw_channel *ch = b->ch;
    if (ch->slot_count == 0u || !ch->slots) return -1;
    b->cover_end = (int64_t)ch->slots[0].at_ms + (int64_t)CTW_DAY_MS;
    for (size_t i = 0; i < ch->slot_count && !b->capped; i++) {
        const ctw_slot *sl = &ch->slots[i];
        int64_t end;
        if (i + 1u < ch->slot_count) {
            end = (int64_t)ch->slots[i + 1u].at_ms;
        } else {
            end = (int64_t)ch->slots[0].at_ms + (int64_t)CTW_DAY_MS;
        }
        b->cursor = (int64_t)sl->at_ms;
        for (size_t j = 0; j < sl->item_count && !b->capped; j++) {
            if (ctw_place_item(b, &sl->items[j], end, (int)i, (int)j)) return -1;
        }
        if (ctw_build_fill_leftover(b, end)) return -1;
    }
    return 0;
}
