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

#include "str_rb.h"

struct str_rbe *str_rb_min (struct str_rb *map)
{
	struct rb_root *root = &map->root;
	struct rb_node *cur = root->rb_node, *parent = NULL;
	struct str_rbe *_min = NULL;

	while (cur) {
		parent = cur;
		cur = parent->rb_left;
		_min = rb_entry (parent, struct str_rbe, rb);
	}
	return _min;
}


struct str_rbe *str_rb_max (struct str_rb *map)
{
	struct rb_root *root = &map->root;
	struct rb_node *cur = root->rb_node, *parent = NULL;
	struct str_rbe *_max = NULL;

	while (cur) {
		parent = cur;
		cur = parent->rb_right;
		_max = rb_entry (parent, struct str_rbe, rb);
	}
	return _max;
}

struct str_rbe *str_rb_find (struct str_rb *map, const char *key, int size)
{
	struct str_rbe *cur;
	struct rb_root *root = &map->root;
	struct rb_node *np = root->rb_node, *parent = NULL;
	int keylen = size, rc;

	while (np) {
		parent = np;
		cur = rb_entry (parent, struct str_rbe, rb);
		keylen = cur->keylen > size ? size : cur->keylen;
		if ((rc = memcmp (cur->key, key, keylen)) == 0 && size == cur->keylen)
			return cur;
		if ((!rc && size < cur->keylen) || rc > 0)
			np = parent->rb_left;
		else if ((!rc && size > cur->keylen) || rc < 0)
			np = parent->rb_right;
	}
	return NULL;
}

struct str_rbe *str_rb_find_leaf (struct str_rb *map, const char *key, int size)
{
	struct str_rbe *cur = 0;
	struct rb_root *root = &map->root;
	struct rb_node *np = root->rb_node, *parent = NULL;
	int keylen = size, rc;

	while (np) {
		parent = np;
		cur = rb_entry (parent, struct str_rbe, rb);
		keylen = cur->keylen > size ? size : cur->keylen;
		if ((rc = memcmp (cur->key, key, keylen)) == 0 && size == cur->keylen)
			return cur;
		if ((!rc && size < cur->keylen) || rc > 0)
			np = parent->rb_left;
		else if ((!rc && size > cur->keylen) || rc < 0)
			np = parent->rb_right;
	}
	return cur;
}


int str_rb_insert (struct str_rb *map, struct str_rbe *entry)
{
	struct str_rbe *cur;
	struct rb_root *root = &map->root;
	struct rb_node **np = &root->rb_node, *parent = NULL;
	int keylen, rc;
	char *key = entry->key;

	map->size++;
	while (*np) {
		parent = *np;
		cur = rb_entry (parent, struct str_rbe, rb);
		keylen = cur->keylen > entry->keylen ? entry->keylen : cur->keylen;
		if ((rc = memcmp (cur->key, key, keylen)) == 0 && entry->keylen == cur->keylen)
			return -1;
		if ((!rc && entry->keylen < cur->keylen) || rc > 0)
			np = &parent->rb_left;
		else if ((!rc && entry->keylen > cur->keylen) || rc < 0)
			np = &parent->rb_right;
	}
	rb_link_node (&entry->rb, parent, np);
	rb_insert_color (&entry->rb, root);
	return 0;
}


void str_rb_delete (struct str_rb *map, struct str_rbe *entry)
{
	map->size--;
	rb_erase (&entry->rb, &map->root);
}
