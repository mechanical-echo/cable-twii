#ifndef CTW_NET_SOCK_H
#define CTW_NET_SOCK_H

#include <stddef.h>
#include <stdint.h>

#define SK_E_FAIL (-1)
#define SK_E_TIMEOUT (-2)

int sk_up(uint32_t timeout_ms);
void sk_down(void);
int sk_connect(const char *host, uint16_t port, uint32_t timeout_ms);
int sk_send_all(int fd, const void *buf, size_t len, uint32_t timeout_ms);
long sk_recv(int fd, void *buf, size_t len, uint32_t timeout_ms);
void sk_close(int fd);

#endif
