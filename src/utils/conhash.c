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
#include "md5.h"
#include "list.h"
#include "alloc.h"
#include "conhash.h"

struct conhash_entry {
	struct str_rbe v_rbe;
	struct list_head item;
	void *owner;
	char md5[16];
	int vsize;
	int vno;
	int size;
	char key[0];
};

static int md5_hash_len (struct conhash_entry *ce)
{
	return 3 * sizeof (int) + ce->size;
}

struct conhash_entry *conhash_ventry_alloc (const char *key, int size, int vsize, int vno)
{
	struct conhash_entry *ce = mem_zalloc (size + sizeof (*ce));
	struct md5_state md5s;
	struct str_rbe *v_rbe;

	if (!ce)
		return 0;
	md5_init (&md5s);
	ce->vsize = vsize;
	ce->vno = vno;
	ce->size = size;
	memcpy (ce->key, key, size);
	v_rbe = &ce->v_rbe;
	v_rbe->key = ce->md5;
	v_rbe->keylen = sizeof (ce->md5);
	md5_process (&md5s, (unsigned char *) &ce->vsize, md5_hash_len (ce));
	md5_done (&md5s, ce->md5);
	return ce;
}

void conhash_entry_free (struct conhash_entry *ce)
{
	mem_free (ce, ce->size + sizeof (*ce));
}

void consistent_hash_init (struct consistent_hash *ch)
{
	str_rb_init (&ch->v_rb_tree);
	INIT_LIST_HEAD (&ch->head);
}

void consistent_hash_destroy (struct consistent_hash *ch)
{
	struct str_rbe *v_rbe;
	struct conhash_entry *ce;

	while ((v_rbe = str_rb_min (&ch->v_rb_tree))) {
		str_rb_delete (&ch->v_rb_tree, v_rbe);
		ce = cont_of (v_rbe, struct conhash_entry, v_rbe);
		conhash_entry_free (ce);
	}
}

int consistent_hash_add (struct consistent_hash *ch, const char *key, int size, void *owner)
{
	int i;
	int rc;
	int vnodes = CONHASH_DEFAULT_VNODES;
	struct conhash_entry *vce;
	struct conhash_entry *tmp;
	struct list_head head = LIST_HEAD_INITIALIZE (head);

	for (i = 0; i < vnodes; i++) {
		if (!(vce = conhash_ventry_alloc (key, size, vnodes, i)))
			break;
		vce->owner = owner;
		if ((rc = str_rb_insert (&ch->v_rb_tree, &vce->v_rbe)) < 0) {
			conhash_entry_free (vce);
			break;
		}
		list_add_tail (&vce->item, &head);
	}
	if (i == vnodes) {
		list_splice (&head, &ch->head);
		return 0;
	}
	walk_each_entry_s (vce, tmp, &head, struct conhash_entry, item) {
		str_rb_delete (&ch->v_rb_tree, &vce->v_rbe);
		conhash_entry_free (vce);
	}
	errno = ENOMEM;
	return -1;
}

void *consistent_hash_get (struct consistent_hash *ch, const char *key, int size)
{
	char md5[16];
	struct md5_state md5s;
	struct str_rbe *rb_entry;
	struct conhash_entry *ce;
	
	md5_init (&md5s);
	md5_process (&md5s, key, size);
	md5_done (&md5s, md5);
	if (!(rb_entry = str_rb_find_leaf (&ch->v_rb_tree, key, size)))
		return 0;
	ce = cont_of (rb_entry, struct conhash_entry, v_rbe);
	return ce->owner;
}

int consistent_hash_rm (struct consistent_hash *ch, const char *key, int size)
{
	struct conhash_entry *vce, *tmp;

	walk_each_entry_s (vce, tmp, &ch->head, struct conhash_entry, item) {
		if (vce->size != size || memcmp (vce->key, key, size) != 0)
			continue;
		list_del_init (&vce->item);
		str_rb_delete (&ch->v_rb_tree, &vce->v_rbe);
		conhash_entry_free (vce);
	}
	return 0;
}


