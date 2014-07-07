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

#ifndef _H_PROXYIO_RB_STR_
#define _H_PROXYIO_RB_STR_

#include "krb.h"
#include <inttypes.h>
#include <string.h>

struct rb_str_node {
	struct rb_node  rb;
	int             keylen;
	char            *key;
	void            *data;
};

struct rb_str {
	int64_t size;
	struct rb_root root;
};

#define rb_str_init(map)	do {		\
		INIT_RB_ROOT(&(map)->root);	\
		(map)->size = 0;		\
	} while (0)

#define rb_str_empty(map) RB_EMPTY_ROOT(&(map)->root)

struct rb_str_node *rb_str_min (struct rb_str *map);

struct rb_str_node *rb_str_max (struct rb_str *map);

struct rb_str_node *rb_str_find (struct rb_str *map, const char *key, int size);

int rb_str_insert (struct rb_str *map, struct rb_str_node *node);

void rb_str_delete (struct rb_str *map, struct rb_str_node *node);


#endif
