#include "icons.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grrlib.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>

#include "draw.h"
#include "textcache.h"
#include "theme.h"
#include "../net/addr.h"
#include "../net/http.h"

#define NET_PREFIX "http://"
#define FETCH_SLOTS 8
#define FETCH_URL_BYTES 320
#define FETCH_PATH_BYTES 512
#define FETCH_MAX_BYTES (256u * 1024u)
#define FETCH_TIMEOUT_MS 5000u
#define FETCH_IDLE_US 10000
#define FETCH_STACK (32 * 1024)
#define FETCH_PRIO 50

typedef enum { FETCH_FREE, FETCH_QUEUED, FETCH_BUSY, FETCH_READY } fetch_state;

typedef struct {
    fetch_state state;
    char url[FETCH_URL_BYTES];
    http_body body;
} fetch_slot;

static tc_cache cache;
static int loads_left;
static fetch_slot slots[FETCH_SLOTS];
static mutex_t lock;
static lwp_t worker;
static bool worker_on;
static bool quit;
static volatile bool cancel;

static void free_icon(void *payload)
{
    GX_DrawDone();
    GRRLIB_FreeTexture((GRRLIB_texImg *)payload);
}

void icons_init(void)
{
    tc_init_capacity(&cache, free_icon, ICON_CACHE_CAPACITY);
}

static int fetch(const char *url, http_body *body)
{
    srv_addr addr;
    char raw[FETCH_PATH_BYTES];
    char path[FETCH_PATH_BYTES];
    if (url_split(url, &addr, raw, sizeof raw) != 0 || url_encode_path(raw, path, sizeof path) != 0) return -1;
    return http_get(&addr, path, FETCH_MAX_BYTES, FETCH_TIMEOUT_MS, &cancel, body);
}

static fetch_slot *next_queued(void)
{
    for (int i = 0; i < FETCH_SLOTS; i++) {
        if (slots[i].state == FETCH_QUEUED) return &slots[i];
    }
    return NULL;
}

static void *fetch_main(void *arg)
{
    (void)arg;
    for (;;) {
        LWP_MutexLock(lock);
        bool stop = quit;
        fetch_slot *s = stop ? NULL : next_queued();
        if (s != NULL) s->state = FETCH_BUSY;
        LWP_MutexUnlock(lock);
        if (stop) break;
        if (s == NULL) {
            usleep(FETCH_IDLE_US);
            continue;
        }
        http_body body = { NULL, 0, 0 };
        if (fetch(s->url, &body) != 0) http_body_free(&body);
        LWP_MutexLock(lock);
        s->body = body;
        s->state = FETCH_READY;
        LWP_MutexUnlock(lock);
    }
    return NULL;
}

static bool start_worker(void)
{
    if (worker_on) return true;
    if (LWP_MutexInit(&lock, false) != 0) return false;
    quit = false;
    cancel = false;
    if (LWP_CreateThread(&worker, fetch_main, NULL, NULL, FETCH_STACK, FETCH_PRIO) != 0) {
        LWP_MutexDestroy(lock);
        return false;
    }
    worker_on = true;
    return true;
}

static void request(const char *url)
{
    if (strlen(url) >= FETCH_URL_BYTES || !start_worker()) return;
    LWP_MutexLock(lock);
    fetch_slot *open = NULL;
    bool pending = false;
    for (int i = 0; i < FETCH_SLOTS; i++) {
        if (slots[i].state == FETCH_FREE) {
            if (open == NULL) open = &slots[i];
        } else if (strcmp(slots[i].url, url) == 0) {
            pending = true;
        }
    }
    if (!pending && open != NULL) {
        snprintf(open->url, sizeof open->url, "%s", url);
        open->state = FETCH_QUEUED;
    }
    LWP_MutexUnlock(lock);
}

static void decode_fetched(void)
{
    for (int i = 0; i < FETCH_SLOTS && loads_left > 0; i++) {
        fetch_slot *s = &slots[i];
        LWP_MutexLock(lock);
        bool ready = s->state == FETCH_READY;
        LWP_MutexUnlock(lock);
        if (!ready) continue;
        loads_left--;
        GRRLIB_texImg *tex = ui_load_png_buffer((const uint8_t *)s->body.data, s->body.len);
        if (tc_insert(&cache, 0, 0, 0, s->url, tex) == NULL && tex != NULL) free_icon(tex);
        http_body_free(&s->body);
        LWP_MutexLock(lock);
        s->state = FETCH_FREE;
        LWP_MutexUnlock(lock);
    }
}

void icons_frame(void)
{
    loads_left = ICON_LOADS_PER_FRAME;
    if (worker_on) decode_fetched();
}

static bool is_net(const char *path)
{
    return strncmp(path, NET_PREFIX, strlen(NET_PREFIX)) == 0;
}

static GRRLIB_texImg *lookup(const char *path)
{
    tc_entry *e = tc_find(&cache, 0, 0, 0, path);
    if (e != NULL) return (GRRLIB_texImg *)e->payload;
    if (is_net(path)) {
        request(path);
        return NULL;
    }
    if (loads_left <= 0) return NULL;
    loads_left--;
    GRRLIB_texImg *tex = ui_load_png_file(path);
    if (tc_insert(&cache, 0, 0, 0, path, tex) == NULL && tex != NULL) {
        free_icon(tex);
        return NULL;
    }
    return tex;
}

void icons_draw(const char *path, float x, float y, float size)
{
    float w = size * ui_pixel_x();
    float ix = x + (size - w) / 2.0f;
    GRRLIB_texImg *tex = path != NULL && path[0] != '\0' ? lookup(path) : NULL;
    if (tex == NULL) {
        ui_panel(ix, y, w, size, size * 0.22f, THEME_PLACEHOLDER_FILL, THEME_PANEL_BORDER);
        return;
    }
    ui_image(tex, ix, y, w, size, 1.0f);
}

void icons_stop(void)
{
    if (!worker_on) return;
    LWP_MutexLock(lock);
    quit = true;
    cancel = true;
    LWP_MutexUnlock(lock);
    LWP_JoinThread(worker, NULL);
    for (int i = 0; i < FETCH_SLOTS; i++) {
        http_body_free(&slots[i].body);
        slots[i].state = FETCH_FREE;
    }
    LWP_MutexDestroy(lock);
    worker_on = false;
}
