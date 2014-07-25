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

#include <string.h>
#include "list.h"
#include "alloc.h"
#include "conhash.h"

struct conhash_entry {
	struct str_rbe rb_entry;
	struct list_head list_item;
	int vsize;
	int vno;
	int size;
	char key[0];
};

struct conhash_entry *conhash_entry_alloc (const char *key, int size)
{
	struct conhash_entry *ce = mem_zalloc (size + sizeof (*ce));
	if (!ce)
		return 0;
	ce->size = size;
	memcpy (ce->key, key, size);
	return ce;
}

void conhash_entry_free (struct conhash_entry *ce)
{
	mem_free (ce, ce->size + sizeof (*ce));
}

void conhash_list_init (struct conhash_list *cl)
{
	str_rb_init (&cl->vce_rb);
}

void conhash_list_destroy (struct conhash_list *cl)
{
	struct str_rbe *rb_entry;
	struct conhash_entry *ce;

	while ((rb_entry = str_rb_min (&cl->vce_rb))) {
		str_rb_delete (&cl->vce_rb, rb_entry);
		ce = cont_of (rb_entry, struct conhash_entry, rb_entry);
		conhash_entry_free (ce);
	}
}

static int conhash_list_add_entry (struct conhash_list *cl, struct conhash_entry *ce)
{
	str_rb_insert (&cl->vce_rb, &ce->rb_entry);
}

static int conhash_list_rm_entry (struct conhash_list *cl, struct conhash_entry *ce)
{
	str_rb_delete (&cl->vce_rb, &ce->rb_entry);
}

int conhash_list_add (struct conhash_list *cl, const char *key, int size, void *owner)
{
	int i, vnodes = CONHASH_DEFAULT_VNODES;
	struct conhash_entry *vce;
	struct conhash_entry *tmp;
	struct list_head head = LIST_HEAD_INITIALIZE (head);

	for (i = 0; i < vnodes; i++) {
		if (!(vce = conhash_entry_alloc (key, size)))
			break;
		vce->vsize = vnodes;
		vce->vno = i;
		conhash_list_add_entry (cl, vce);
		list_add_tail (&vce->list_item, &head);
	}
	if (i == vnodes)
		return 0;
	walk_each_entry_s (vce, tmp, &head, struct conhash_entry, list_item) {
		conhash_list_rm_entry (cl, vce);
		conhash_entry_free (vce);
	}
	errno = ENOMEM;
	return -1;
}

int conhash_list_rm (struct conhash_list *cl, const char *key, int size)
{
}

void *conhash_list_get (struct conhash_list *cl, const char *key, int size);
