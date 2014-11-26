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

#ifndef _H_PROXYIO_SP_PUBSUB_INTERN_
#define _H_PROXYIO_SP_PUBSUB_INTERN_

#include <sp/sp_hdr.h>

struct pubsub_tgtd {
    struct tgtd tg;
    struct msgbuf_head ls_head;   /* local storage */
};

static inline void pubsub_tgtd_free(struct pubsub_tgtd* ps_tg) {
    mem_free(ps_tg, sizeof(struct pubsub_tgtd));
}

static inline struct pubsub_tgtd* get_pubsub_tgtd(struct tgtd* tg) {
    return cont_of(tg, struct pubsub_tgtd, tg);
}

#endif
