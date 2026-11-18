#include "netsrc.h"

#include <stdlib.h>
#include <unistd.h>
#include <ogc/irq.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>

#include "netring.h"
#include "net/addr.h"
#include "net/http.h"

#define NS_RING_BYTES (2u * 1024u * 1024u)
#define NS_AHEAD_BYTES (1280u * 1024u)
#define NS_CHUNK (16u * 1024u)
#define NS_PATH_BYTES 512
#define NS_SLICE_MS 50u
#define NS_IDLE_US 5000
#define NS_WAIT_US 2000
#define NS_STACK (32 * 1024)
#define NS_PRIO 56

struct ns_source {
    http_stream hs;
    nr_ring ring;
    mutex_t lock;
    lwp_t thread;
    uint64_t length;
    uint64_t pos;
    uint32_t gen;
    uint32_t timeout_ms;
    u64 wait_since;
    bool waiting;
    bool failed;
    bool quit;
    uint8_t stage[NS_CHUNK];
};

static uint8_t *ring_mem;
static bool ring_taken;
#ifdef CTW_DEBUG_OVERLAY
static volatile uint32_t received;
#endif

static uint32_t since_ms(u64 start)
{
    return (uint32_t)ticks_to_millisecs(diff_ticks(start, gettime()));
}

static uint8_t *ring_memory(void)
{
    if (ring_mem != NULL) return ring_mem;
    u32 level = IRQ_Disable();
    uintptr_t lo = ((uintptr_t)SYS_GetArena2Lo() + 31u) & ~(uintptr_t)31u;
    if (lo + NS_RING_BYTES <= (uintptr_t)SYS_GetArena2Hi()) {
        SYS_SetArena2Lo((void *)(lo + NS_RING_BYTES));
        ring_mem = (uint8_t *)lo;
    }
    IRQ_Restore(level);
    return ring_mem;
}

static int start_at(ns_source *s, uint64_t pos)
{
    s->hs.timeout_ms = s->timeout_ms;
    int e = hs_start(&s->hs, pos);
    s->hs.timeout_ms = NS_SLICE_MS;
    return e;
}

static void commit(ns_source *s, uint32_t gen, size_t n)
{
    LWP_MutexLock(s->lock);
    if (s->gen == gen) nr_append(&s->ring, s->stage, n);
    LWP_MutexUnlock(s->lock);
#ifdef CTW_DEBUG_OVERLAY
    received += (uint32_t)n;
#endif
}

static void give_up(ns_source *s, uint32_t gen)
{
    hs_close(&s->hs);
    LWP_MutexLock(s->lock);
    if (s->gen == gen) s->failed = true;
    LWP_MutexUnlock(s->lock);
}

static void *fetch_main(void *arg)
{
    ns_source *s = arg;
    uint32_t gen = 0;
    bool live = false;
    bool retried = false;
    u64 last = gettime();
    for (;;) {
        LWP_MutexLock(s->lock);
        if (s->gen != gen) {
            gen = s->gen;
            live = false;
            retried = false;
        }
        bool quit = s->quit;
        uint64_t end = nr_end(&s->ring);
        size_t room = nr_space(&s->ring);
        bool hold = s->failed || end >= s->length || room == 0 || (end > s->pos && end - s->pos >= NS_AHEAD_BYTES);
        LWP_MutexUnlock(s->lock);
        if (quit) break;
        if (hold) {
            usleep(NS_IDLE_US);
            last = gettime();
            continue;
        }
        if (!live) {
            live = start_at(s, end) == 0;
            last = gettime();
            if (live) continue;
        } else {
            long n = hs_read(&s->hs, s->stage, room < NS_CHUNK ? room : NS_CHUNK);
            if (n > 0) {
                commit(s, gen, (size_t)n);
                retried = false;
                last = gettime();
                continue;
            }
            if (n == HTTP_E_TIMEOUT && since_ms(last) < s->timeout_ms) continue;
            live = false;
        }
        if (!retried) {
            retried = true;
            continue;
        }
        retried = false;
        give_up(s, gen);
    }
    return NULL;
}

ns_source *ns_open(const char *url, uint32_t timeout_ms)
{
    srv_addr addr;
    char raw[NS_PATH_BYTES];
    char path[NS_PATH_BYTES];
    if (ring_taken || url_split(url, &addr, raw, sizeof raw) != 0 || url_encode_path(raw, path, sizeof path) != 0) {
        return NULL;
    }
    uint8_t *mem = ring_memory();
    if (mem == NULL) return NULL;
    ns_source *s = calloc(1, sizeof *s);
    if (s == NULL) return NULL;
    if (hs_open(&s->hs, &addr, path, timeout_ms) != 0) {
        free(s);
        return NULL;
    }
    s->length = s->hs.length;
    s->timeout_ms = timeout_ms;
    s->hs.timeout_ms = NS_SLICE_MS;
    s->gen = 1;
    nr_init(&s->ring, mem, NS_RING_BYTES);
    if (LWP_MutexInit(&s->lock, false) != 0) {
        hs_close(&s->hs);
        free(s);
        return NULL;
    }
    if (LWP_CreateThread(&s->thread, fetch_main, s, NULL, NS_STACK, NS_PRIO) != 0) {
        LWP_MutexDestroy(s->lock);
        hs_close(&s->hs);
        free(s);
        return NULL;
    }
    ring_taken = true;
    return s;
}

size_t ns_read(ns_source *s, uint8_t *dst, size_t len)
{
    size_t n = 0;
    bool blocked = false;
    LWP_MutexLock(s->lock);
    while (!s->quit && len > 0 && s->pos < s->length) {
        if (!nr_contains(&s->ring, s->pos) && s->pos != nr_end(&s->ring)) {
            nr_reset(&s->ring, s->pos);
            s->gen++;
            s->failed = false;
        }
        nr_drop_before(&s->ring, s->pos);
        n = nr_copy(&s->ring, s->pos, dst, len);
        if (n > 0 || s->failed) break;
        if (!s->waiting) {
            s->waiting = true;
            s->wait_since = gettime();
        }
        blocked = true;
        LWP_MutexUnlock(s->lock);
        usleep(NS_WAIT_US);
        LWP_MutexLock(s->lock);
    }
    s->pos += n;
    if ((n > 0 && !blocked) || s->pos >= s->length) s->waiting = false;
    LWP_MutexUnlock(s->lock);
    return n;
}

void ns_seek(ns_source *s, uint64_t pos)
{
    LWP_MutexLock(s->lock);
    s->pos = pos;
    s->failed = false;
    LWP_MutexUnlock(s->lock);
}

uint64_t ns_tell(const ns_source *s)
{
    LWP_MutexLock(s->lock);
    uint64_t pos = s->pos;
    LWP_MutexUnlock(s->lock);
    return pos;
}

uint64_t ns_length(const ns_source *s)
{
    return s->length;
}

bool ns_failed(const ns_source *s)
{
    LWP_MutexLock(s->lock);
    bool failed = s->failed;
    LWP_MutexUnlock(s->lock);
    return failed;
}

uint32_t ns_waiting_ms(const ns_source *s)
{
    LWP_MutexLock(s->lock);
    uint32_t ms = s->waiting ? since_ms(s->wait_since) : 0;
    LWP_MutexUnlock(s->lock);
    return ms;
}

void ns_cancel(ns_source *s)
{
    LWP_MutexLock(s->lock);
    s->quit = true;
    LWP_MutexUnlock(s->lock);
}

void ns_close(ns_source *s)
{
    if (s == NULL) return;
    ns_cancel(s);
    LWP_JoinThread(s->thread, NULL);
    hs_close(&s->hs);
    LWP_MutexDestroy(s->lock);
    ring_taken = false;
    free(s);
}

#ifdef CTW_DEBUG_OVERLAY
uint32_t ns_bytes_received(void)
{
    return received;
}
#endif
