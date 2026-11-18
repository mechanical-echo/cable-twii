#ifndef CTW_MEDIA_NETSRC_H
#define CTW_MEDIA_NETSRC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ns_source ns_source;

ns_source *ns_open(const char *url, uint32_t timeout_ms);
size_t ns_read(ns_source *s, uint8_t *dst, size_t len);
void ns_seek(ns_source *s, uint64_t pos);
uint64_t ns_tell(const ns_source *s);
uint64_t ns_length(const ns_source *s);
bool ns_failed(const ns_source *s);
uint32_t ns_waiting_ms(const ns_source *s);
void ns_cancel(ns_source *s);
void ns_close(ns_source *s);
#ifdef CTW_DEBUG_OVERLAY
uint32_t ns_bytes_received(void);
#endif

#endif
