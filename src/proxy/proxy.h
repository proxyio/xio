#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include <channel/channel.h>

struct pyio;

struct pyio *pyio_new();
int pyio_close(struct pyio *py);

int pyio_bind(struct pyio *py, const char *addr);
int pyio_connect(struct pyio *py, const char *addr);
char *pyio_recv(struct pyio *py);
int pyio_send(struct pyio *py, char *payload);

static inline char *pyio_allocmsg(uint32_t size) {
    return channel_allocmsg(size);
}

static inline void pyio_freemsg(char *payload) {
    return channel_freemsg(payload);
}


#endif
