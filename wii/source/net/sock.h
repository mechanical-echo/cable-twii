#ifndef CTW_NET_SOCK_H
#define CTW_NET_SOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SK_E_FAIL (-1)
#define SK_E_TIMEOUT (-2)
#define SK_E_CANCEL (-3)

int sk_up(uint32_t timeout_ms);
void sk_down(void);
int sk_connect(const char *host, uint16_t port, uint32_t timeout_ms, const volatile bool *cancel);
int sk_send_all(int fd, const void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel);
long sk_recv(int fd, void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel);
void sk_close(int fd);

#endif
