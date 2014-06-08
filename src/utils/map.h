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

#ifndef _XIO_MAP_
#define _XIO_MAP_

#include "krb.h"
#include <string.h>

typedef struct ssmap_node {
	struct rb_node  rb;
	int             keylen;
	char            *key;
	void            *data;
} ssmap_node_t;

typedef struct ssmap {
	int64_t size;
	struct rb_root root;
} ssmap_t;

#define ssmap_init(map)	do {			\
	INIT_RB_ROOT(&(map)->root);		\
	(map)->size = 0;			\
    } while (0)

#define ssmap_empty(map) RB_EMPTY_ROOT(&(map)->root)


static inline ssmap_node_t *ssmap_min(ssmap_t *map)
{
	struct rb_root *root = &map->root;
	struct rb_node *cur = root->rb_node, *parent = NULL;
	ssmap_node_t *_min = NULL;

	while (cur) {
		parent = cur;
		cur = parent->rb_left;
		_min = rb_entry(parent, ssmap_node_t, rb);
	}
	return _min;
}


static inline ssmap_node_t *ssmap_max(ssmap_t *map)
{
	struct rb_root *root = &map->root;
	struct rb_node *cur = root->rb_node, *parent = NULL;
	ssmap_node_t *_max = NULL;

	while (cur) {
		parent = cur;
		cur = parent->rb_right;
		_max = rb_entry(parent, ssmap_node_t, rb);
	}
	return _max;
}

static inline ssmap_node_t *ssmap_find(ssmap_t *map, const char *key, int size)
{
	ssmap_node_t *cur;
	struct rb_root *root = &map->root;
	struct rb_node **np = &root->rb_node, *parent = NULL;
	int keylen = size, rc;

	while (*np) {
		parent = *np;
		cur = rb_entry(parent, ssmap_node_t, rb);
		keylen = cur->keylen > size ? size : cur->keylen;
		if ((rc = memcmp(cur->key, key, keylen)) == 0 && size == cur->keylen)
			return cur;
		if ((!rc && size < cur->keylen) || rc > 0)
			np = &parent->rb_left;
		else if ((!rc && size > cur->keylen) || rc < 0)
			np = &parent->rb_right;
	}
	return NULL;
}

static inline int ssmap_insert(ssmap_t *map, ssmap_node_t *node)
{
	ssmap_node_t *cur;
	struct rb_root *root = &map->root;
	struct rb_node **np = &root->rb_node, *parent = NULL;
	int keylen, rc;
	char *key = node->key;

	map->size++;
	while (*np) {
		parent = *np;
		cur = rb_entry(parent, ssmap_node_t, rb);
		keylen = cur->keylen > node->keylen ? node->keylen : cur->keylen;
		if ((rc = memcmp(cur->key, key, keylen)) == 0 && node->keylen == cur->keylen)
			return -1;
		if ((!rc && node->keylen < cur->keylen) || rc > 0)
			np = &parent->rb_left;
		else if ((!rc && node->keylen > cur->keylen) || rc < 0)
			np = &parent->rb_right;
	}
	rb_link_node(&node->rb, parent, np);
	rb_insert_color(&node->rb, root);
	return 0;
}


static inline void ssmap_delete(ssmap_t *map, ssmap_node_t *node)
{
	map->size--;
	rb_erase(&node->rb, &map->root);
}

#endif
