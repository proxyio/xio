/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#ifndef _H_PROXYIO_TCP_INTERN_
#define _H_PROXYIO_TCP_INTERN_

#include <rex/rex.h>
#include "../sockbase.h"



enum {
    /* Following msgbuf_head events are provided by sockbase */
    EV_SNDBUF_ADD        =     0x001,
    EV_SNDBUF_RM         =     0x002,
    EV_SNDBUF_EMPTY      =     0x004,
    EV_SNDBUF_NONEMPTY   =     0x008,
    EV_SNDBUF_FULL       =     0x010,
    EV_SNDBUF_NONFULL    =     0x020,

    EV_RCVBUF_ADD        =     0x101,
    EV_RCVBUF_RM         =     0x102,
    EV_RCVBUF_EMPTY      =     0x104,
    EV_RCVBUF_NONEMPTY   =     0x108,
    EV_RCVBUF_FULL       =     0x110,
    EV_RCVBUF_NONFULL    =     0x120,
};


struct sio {
    struct io ops;
    struct sockbase base;
    struct ev_sig sig;
    struct ev_loop* el;
    struct ev_fd et;
    struct rex_sock s;
    struct bio rbuf;
    struct {
        u64 binded: 1;
        u64 send_lock: 1;
    } flagset;
};

static inline int sio_lock_send(struct sio* tcps) {
    int locked = 0;
    struct sockbase* sb = &tcps->base;

    mutex_lock(&sb->lock);

    if (!tcps->flagset.send_lock) {
        tcps->flagset.send_lock = 1;
        locked = 1;
    }

    mutex_unlock(&sb->lock);
    return locked;
}

int sio_setopt(struct sockbase* sb, int opt, void* optval, int optlen);
int sio_getopt(struct sockbase* sb, int opt, void* optval, int* optlen);

extern struct sockbase_vfptr tcp_listener_vfptr;
extern struct sockbase_vfptr tcp_connector_vfptr;
extern struct sockbase_vfptr ipc_listener_vfptr;
extern struct sockbase_vfptr ipc_connector_vfptr;

#endif
