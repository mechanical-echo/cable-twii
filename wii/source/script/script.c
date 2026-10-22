#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#define SCRIPT_FILE "debug-script.txt"
#define MAX_STEPS 256
#define FRAME_MS 16u

typedef enum { ST_IR, ST_IR_OFF, ST_PRESS, ST_DOWN, ST_UP } step_kind;

typedef struct {
    uint32_t at_ms;
    step_kind kind;
    uint32_t button;
    float x;
    float y;
} step;

static step steps[MAX_STEPS];
static size_t step_count;
static size_t next_step;
static bool active;
static uint32_t now_ms;
static uint32_t held;
static uint32_t down;
static uint32_t up;
static uint32_t release_next;
static bool ir_on;
static float ir_x, ir_y;

static uint32_t button_mask(const char *name)
{
    static const struct { const char *name; uint32_t mask; } names[] = {
        { "A", WPAD_BUTTON_A }, { "B", WPAD_BUTTON_B }, { "UP", WPAD_BUTTON_UP }, { "DOWN", WPAD_BUTTON_DOWN },
        { "LEFT", WPAD_BUTTON_LEFT }, { "RIGHT", WPAD_BUTTON_RIGHT }, { "PLUS", WPAD_BUTTON_PLUS },
        { "MINUS", WPAD_BUTTON_MINUS }, { "HOME", WPAD_BUTTON_HOME }, { "ONE", WPAD_BUTTON_1 }, { "TWO", WPAD_BUTTON_2 },
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (strcmp(names[i].name, name) == 0) return names[i].mask;
    }
    return 0;
}

static void parse_line(char *line)
{
    if (step_count >= MAX_STEPS) return;
    char cmd[16] = "", arg1[16] = "", arg2[16] = "";
    unsigned at = 0;
    int n = sscanf(line, "%u %15s %15s %15s", &at, cmd, arg1, arg2);
    if (n < 2) return;
    step *s = &steps[step_count];
    memset(s, 0, sizeof *s);
    s->at_ms = at;
    if (strcmp(cmd, "ir") == 0) {
        if (strcmp(arg1, "off") == 0) {
            s->kind = ST_IR_OFF;
        } else {
            s->kind = ST_IR;
            s->x = (float)atof(arg1);
            s->y = (float)atof(arg2);
        }
    } else if (strcmp(cmd, "press") == 0 || strcmp(cmd, "down") == 0 || strcmp(cmd, "up") == 0) {
        s->kind = cmd[0] == 'p' ? ST_PRESS : (cmd[0] == 'd' ? ST_DOWN : ST_UP);
        s->button = button_mask(arg1);
        if (s->button == 0) return;
    } else {
        return;
    }
    step_count++;
}

void dbg_script_load(const char *root)
{
    char path[320];
    snprintf(path, sizeof path, "%s/" SCRIPT_FILE, root);
    FILE *f = fopen(path, "r");
    if (f == NULL) return;
    char line[128];
    while (fgets(line, sizeof line, f) != NULL) parse_line(line);
    fclose(f);
    active = step_count > 0;
}

void dbg_script_poll(void)
{
    if (!active) return;
    now_ms += FRAME_MS;
    down = 0;
    up = release_next;
    held &= ~release_next;
    release_next = 0;
    while (next_step < step_count && steps[next_step].at_ms <= now_ms) {
        const step *s = &steps[next_step++];
        switch (s->kind) {
        case ST_IR:
            ir_on = true;
            ir_x = s->x;
            ir_y = s->y;
            break;
        case ST_IR_OFF:
            ir_on = false;
            break;
        case ST_PRESS:
            down |= s->button;
            held |= s->button;
            release_next |= s->button;
            break;
        case ST_DOWN:
            down |= s->button;
            held |= s->button;
            break;
        case ST_UP:
            up |= s->button;
            held &= ~s->button;
            break;
        }
    }
}

bool dbg_script_active(void)
{
    return active;
}

uint32_t dbg_script_down(void)
{
    return down;
}

uint32_t dbg_script_held(void)
{
    return held;
}

uint32_t dbg_script_up(void)
{
    return up;
}

bool dbg_script_ir(float *x, float *y)
{
    if (!ir_on) return false;
    *x = ir_x;
    *y = ir_y;
    return true;
}
