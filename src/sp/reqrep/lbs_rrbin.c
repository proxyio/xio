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

#include "req_ep.h"

struct lb_strategy_rrbin {
	struct lbs_vfptr lb_strategy;
};

static struct tgtd *weight_rrbin_select (struct reqep *reqep, char *ubuf)
{
	struct epbase *ep = &reqep->base;
	struct tgtd *tg;
	struct req_tgtd *req_tg;

	if (list_empty (&ep->connectors))
		return 0;
	tg = list_first (&ep->connectors, struct tgtd, item);
	req_tg = cont_of (tg, struct req_tgtd, tg);

	/* Move to the tail if current_weight less than zero */
	if (--req_tg->algod.rrbin.current_weight <= 0) {
		req_tg->algod.rrbin.current_weight = req_tg->algod.rrbin.origin_weight;
		list_move_tail (&tg->item, &ep->connectors);
	}
	return tg;
}

static struct lbs_vfptr *weight_rrbin_new ()
{
	struct lb_strategy_rrbin *rr = mem_zalloc (sizeof (struct lb_strategy_rrbin));
	rr->lb_strategy = *rrbin_vfptr;
	return &rr->lb_strategy;
}

static void weight_rrbin_free (struct lbs_vfptr *lb_strategy)
{
	struct lb_strategy_rrbin *rr = cont_of (lb_strategy, struct lb_strategy_rrbin, lb_strategy);
	mem_free (rr, sizeof (*rr));
}


struct lbs_vfptr weight_rrbin_ops = {
	.type = SP_REQ_RRBIN,
	.new = weight_rrbin_new,
	.free = weight_rrbin_free,
	.select = weight_rrbin_select,
};

struct lbs_vfptr *rrbin_vfptr = &weight_rrbin_ops;

