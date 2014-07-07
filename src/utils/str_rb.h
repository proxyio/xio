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

#ifndef _H_PROXYIO_STR_RB_
#define _H_PROXYIO_STR_RB_

#include "krb.h"
#include <inttypes.h>
#include <string.h>

struct str_rbe {
	struct rb_node  rb;
	int             keylen;
	char            *key;
	void            *data;
};

struct str_rb {
	int64_t size;
	struct rb_root root;
};

#define str_rb_init(map)	do {		\
		INIT_RB_ROOT(&(map)->root);	\
		(map)->size = 0;		\
	} while (0)

#define str_rb_empty(map) RB_EMPTY_ROOT(&(map)->root)

struct str_rbe *str_rb_min (struct str_rb *map);

struct str_rbe *str_rb_max (struct str_rb *map);

struct str_rbe *str_rb_find (struct str_rb *map, const char *key, int size);

int str_rb_insert (struct str_rb *map, struct str_rbe *entry);

void str_rb_delete (struct str_rb *map, struct str_rbe *entry);


#endif
