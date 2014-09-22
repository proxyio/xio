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

#include <utils/consistent_hash.h>
#include "req_ep.h"

struct conhash_lbs {
	struct loadbalance_vfptr vf;
	struct reqep *owner;
	struct consistent_hash tgs;
};

#define cont_of_conhash(lbs)			\
	cont_of (lbs, struct conhash_lbs, vf);

static struct req_tgtd *conhash_select (struct loadbalance_vfptr *lbs, char *ubuf)
{
	struct conhash_lbs *cl = cont_of_conhash (lbs);
	int size = MIN (usize (ubuf), 16);

	return (struct req_tgtd *) consistent_hash_get (&cl->tgs, ubuf, size);
}

static struct loadbalance_vfptr *conhash_new (struct reqep *reqep, ...)
{
	struct conhash_lbs *cl = mem_zalloc (sizeof (struct conhash_lbs));

	cl->owner = reqep;
	cl->vf = *conhash_vfptr;
	consistent_hash_init (&cl->tgs);
	return &cl->vf;
}

static void conhash_free (struct loadbalance_vfptr *lbs)
{
	struct conhash_lbs *cl = cont_of_conhash (lbs);

	consistent_hash_destroy (&cl->tgs);
	mem_free (cl, sizeof (*cl));
}

static void conhash_add (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
	struct conhash_lbs *cl = cont_of_conhash (lbs);
	consistent_hash_add (&cl->tgs, (const char *) tg->uuid, sizeof (tg->uuid), tg);
}

static void conhash_rm (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
	struct conhash_lbs *cl = cont_of_conhash (lbs);
	consistent_hash_rm (&cl->tgs, (const char *) tg->uuid, sizeof (tg->uuid));
}


struct loadbalance_vfptr conhash_ops = {
	.type = SP_REQ_CONSISTENT_HASH,
	.new = conhash_new,
	.free = conhash_free,
	.add = conhash_add,
	.rm = conhash_rm,
	.select = conhash_select,
};

struct loadbalance_vfptr *conhash_vfptr = &conhash_ops;

