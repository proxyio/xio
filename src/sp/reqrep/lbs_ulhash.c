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

#include <stdarg.h>
#include "req_ep.h"

struct ulhash_lbs {
	struct loadbalance_vfptr vf;
	struct reqep *owner;
	ulhash hash;
};

#define cont_of_ulhash(lbs)				\
	cont_of (lbs, struct ulhash_lbs, vf);

static struct req_tgtd *ulhash_select (struct loadbalance_vfptr *lbs, char *ubuf)
{
	struct ulhash_lbs *ul = cont_of_ulhash (lbs);
	struct req_tgtd *req_tg;

	return 0;
}

static struct loadbalance_vfptr *ulhash_new (struct reqep *reqep, ...)
{
	struct ulhash_lbs *ul = mem_zalloc (sizeof (struct ulhash_lbs));
	va_list ap;

	ul->owner = reqep;
	ul->vf = *ulhash_vfptr;
	va_start (ap, reqep);
	ul->hash = va_arg (ap, ulhash);
	va_end (ap);
	return &ul->vf;
}

static void ulhash_free (struct loadbalance_vfptr *lbs)
{
	struct ulhash_lbs *ul = cont_of_ulhash (lbs);
	mem_free (ul, sizeof (*ul));
}

static void ulhash_add (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
}

static void ulhash_rm (struct loadbalance_vfptr *lbs, struct req_tgtd *tg)
{
}


struct loadbalance_vfptr ulhash_ops = {
	.type = SP_REQ_ULHASH,
	.new = ulhash_new,
	.free = ulhash_free,
	.add = ulhash_add,
	.rm = ulhash_rm,
	.select = ulhash_select,
};

struct loadbalance_vfptr *ulhash_vfptr = &ulhash_ops;

