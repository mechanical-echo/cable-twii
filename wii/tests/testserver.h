#ifndef CTW_TESTS_TESTSERVER_H
#define CTW_TESTS_TESTSERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *head;
    const void *body;
    size_t body_len;
    bool close_after;
} ts_reply;

uint16_t ts_start(const ts_reply *replies, size_t n);
const char *ts_request(size_t i);
size_t ts_conn(size_t i);
void ts_stop(void);

#endif
