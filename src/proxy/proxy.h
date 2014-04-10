#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include <channel/channel.h>

struct proxyio;

struct proxyio *proxyio_new();
int proxy_close(struct proxyio *py);

int proxy_bind(struct proxyio *py, const char *addr);
int proxy_connect(struct proxyio *py, const char *addr);
char *proxy_recv(struct proxyio *py);
int proxy_send(struct proxyio *py, char *payload);

static inline char *proxy_allocmsg(uint32_t size) {
    return channel_allocmsg(size);
}

static inline void proxy_freemsg(char *payload) {
    return channel_freemsg(payload);
}


#endif
