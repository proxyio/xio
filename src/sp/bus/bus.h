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

#ifndef _SP_BUS_
#define _SP_BUS_


#include <xio/sp_bus.h>
#include <sp/sp_module.h>

struct bus_ep {
	struct epbase base;
};

struct bus_tgtd {
	struct tgtd tg;
	struct skbuf_head ls_head;   /* local storage */
};

static inline void bus_tgtd_free(struct bus_tgtd *ps_tg) {
	mem_free (ps_tg, sizeof (struct bus_tgtd) );
}

static inline struct bus_tgtd *get_bus_tgtd (struct tgtd *tg) {
	return cont_of (tg, struct bus_tgtd, tg);
}


#endif
