#include <stdio.h>
#include <os/alloc.h>
#include <hash/crc.h>
#include "xmsg.h"


u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return (char *)&msg->vec;
}

char *xallocmsg(int size) {
    struct xmsg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return 0;
    msg = (struct xmsg *)chunk;
    msg->vec.size = size;
    msg->vec.checksum = crc16((char *)&msg->vec.size, 4);
    return msg->vec.chunk;
}

void xfreemsg(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    mem_free(msg, sizeof(*msg) + msg->vec.size);
}

int xmsglen(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return msg->vec.size;
}

