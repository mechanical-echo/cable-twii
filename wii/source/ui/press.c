#include "press.h"

int ps_update(ps_state *s, int hot, bool pressed, bool held, bool released)
{
    if (pressed) {
        s->armed = hot;
        return 0;
    }
    if (released) {
        int fired = s->armed != 0 && s->armed == hot ? s->armed : 0;
        s->armed = 0;
        return fired;
    }
    if (!held) s->armed = 0;
    return 0;
}

int ps_pressed(const ps_state *s, int hot)
{
    return s->armed != 0 && s->armed == hot ? hot : 0;
}
