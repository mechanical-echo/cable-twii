#include "loader.h"

#include <stdbool.h>
#include <string.h>
#include <ogc/lwp.h>

#include "net/sock.h"

#define LD_UP_TRIES 3
#define LD_UP_MS 10000u
#define LD_REQUEST_MS 5000u
#define LD_STACK (128 * 1024)
#define LD_PRIO 40

typedef enum { LD_JOB_LOAD, LD_JOB_INFO } ld_job;

static ld_state state = LD_IDLE;
static ld_job job;
static srv_addr addr;
static lwp_t thread;
static volatile bool finished;
static bool online;
static srv_result result;
static ctw_db *db;
static ctw_issues issues;
static srv_info info;

static bool bring_up(void)
{
    for (int i = 0; i < LD_UP_TRIES && !online; i++) {
        if (sk_up(LD_UP_MS) == 0) {
            online = true;
        } else {
            sk_down();
        }
    }
    return online;
}

static void *run(void *arg)
{
    (void)arg;
    if (!bring_up()) {
        result = SRV_NO_NETWORK;
    } else if (job == LD_JOB_LOAD) {
        result = srv_load_db(&addr, LD_REQUEST_MS, &db, &issues, &info);
    } else {
        result = srv_fetch_info(&addr, LD_REQUEST_MS, &info);
    }
    finished = true;
    return NULL;
}

static void drop_result(void)
{
    ctw_db_free(db);
    db = NULL;
    ctw_issues_free(&issues);
    memset(&info, 0, sizeof info);
}

static void start(ld_job j, const srv_addr *a)
{
    if (state == LD_RUNNING) return;
    drop_result();
    job = j;
    addr = *a;
    result = SRV_NO_ANSWER;
    finished = false;
    state = LWP_CreateThread(&thread, run, NULL, NULL, LD_STACK, LD_PRIO) == 0 ? LD_RUNNING : LD_DONE;
}

void ld_load(const srv_addr *a)
{
    start(LD_JOB_LOAD, a);
}

void ld_info(const srv_addr *a)
{
    start(LD_JOB_INFO, a);
}

ld_state ld_poll(void)
{
    if (state == LD_RUNNING && finished) {
        LWP_JoinThread(thread, NULL);
        state = LD_DONE;
    }
    return state;
}

srv_result ld_take_db(ctw_db **out, ctw_issues *out_issues, srv_info *out_info)
{
    *out = NULL;
    memset(out_issues, 0, sizeof *out_issues);
    memset(out_info, 0, sizeof *out_info);
    if (ld_poll() != LD_DONE || job != LD_JOB_LOAD) return SRV_NO_ANSWER;
    *out = db;
    *out_issues = issues;
    *out_info = info;
    db = NULL;
    memset(&issues, 0, sizeof issues);
    state = LD_IDLE;
    return result;
}

srv_result ld_take_info(srv_info *out)
{
    memset(out, 0, sizeof *out);
    if (ld_poll() != LD_DONE || job != LD_JOB_INFO) return SRV_NO_ANSWER;
    *out = info;
    state = LD_IDLE;
    return result;
}
