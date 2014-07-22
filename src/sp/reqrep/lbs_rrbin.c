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

struct rrbin_lbs {
	struct loadbalance_vfptr vf;
	struct reqep *owner;
	struct list_head head;
};

static struct req_tgtd *weight_rrbin_select (struct loadbalance_vfptr *lbs, char *ubuf)
{
	struct rrbin_lbs *rr = cont_of (lbs, struct rrbin_lbs, vf);
	struct req_tgtd *tg;

	if (list_empty (&rr->head))
		return 0;

	tg = list_first (&rr->head, struct req_tgtd, algod.rrbin.item);
	
	/* Move to the tail if current_weight less than zero */
	if (--tg->algod.rrbin.current_weight <= 0) {
		tg->algod.rrbin.current_weight = tg->algod.rrbin.origin_weight;
		list_move_tail (&tg->algod.rrbin.item, &rr->head);
	}
	return tg;
}

static struct loadbalance_vfptr *weight_rrbin_new (struct reqep *reqep, ...)
{
	struct rrbin_lbs *rr = mem_zalloc (sizeof (struct rrbin_lbs));
	rr->owner = reqep;	
	rr->vf = *rrbin_vfptr;
	INIT_LIST_HEAD (&rr->head);
	return &rr->vf;
}

static void weight_rrbin_free (struct loadbalance_vfptr *lbs)
{
	struct rrbin_lbs *rr = cont_of (lbs, struct rrbin_lbs, vf);
	mem_free (rr, sizeof (*rr));
}

static void weight_rrbin_add (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
	struct rrbin_lbs *rr = cont_of (lbs, struct rrbin_lbs, vf);
	tg->algod.rrbin.origin_weight = 1;
	tg->algod.rrbin.current_weight = 1;
	INIT_LIST_HEAD (&tg->algod.rrbin.item);
	list_add_tail (&tg->algod.rrbin.item, &rr->head);
}

static void weight_rrbin_rm (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
	list_del_init (&tg->algod.rrbin.item);
}

struct loadbalance_vfptr weight_rrbin_ops = {
	.type = SP_REQ_RRBIN,
	.new = weight_rrbin_new,
	.free = weight_rrbin_free,
	.add = weight_rrbin_add,
	.rm = weight_rrbin_rm,
	.select = weight_rrbin_select,
};

struct loadbalance_vfptr *rrbin_vfptr = &weight_rrbin_ops;

