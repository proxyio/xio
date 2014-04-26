#ifndef _HPIO_PROTOCOL_
#define _HPIO_PROTOCOL_

#include <base.h>
#include <ds/list.h>

struct xsock_protocol {
    int type;
    int pf;
    int (*bind) (int xd, const char *sock);
    void (*close) (int xd);
    void (*snd_notify) (int xd, u32 events);
    void (*rcv_notify) (int xd, u32 events);
    struct list_head link;
};

extern const char *xprotocol_str[];

#endif
