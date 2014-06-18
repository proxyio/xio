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
	struct algo_ops *target_algo;
	struct repep *peer;
};

#define req_ep(ep) cont_of(ep, struct reqep, base)
#define peer_repep(qep) (cont_of(qep, struct reqep, base))->peer

extern int epbase_proxyto (struct epbase *repep, struct epbase *reqep);


struct algo_ops {
	int type;
	struct tgtd * (*select) (struct reqep *reqep, char *ubuf);
};

extern struct algo_ops *rrbin_vfptr;


#endif
