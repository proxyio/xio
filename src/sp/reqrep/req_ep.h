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

#ifndef _H_PROXYIO_SP_REQ_
#define _H_PROXYIO_SP_REQ_

#include <sp/sp_module.h>
#include "rr.h"

struct repep;

struct reqep {
	struct epbase base;
	struct repep *peer;
	struct lbs_vfptr *target_algo;
};

#define peer_repep(qep) (cont_of(qep, struct reqep, base))->peer

extern int epbase_proxyto (struct epbase *repep, struct epbase *reqep);


struct lbs_vfptr {
	int type;
	struct tgtd * (*select) (struct reqep *reqep, char *ubuf);
};

struct req_tgtd {
	struct tgtd tg;
	uuid_t uuid;                 /* global unique id for distributed system */
	struct msgbuf_head ls_head;   /* local storage */
	union {
		struct {
			int origin_weight;
			int cur_weight;
		} rrbin;
	} algod;
};

static inline struct req_tgtd *get_req_tgtd (struct tgtd *tg) {
	return cont_of (tg, struct req_tgtd, tg);
}

extern struct lbs_vfptr *rrbin_vfptr;

#endif
