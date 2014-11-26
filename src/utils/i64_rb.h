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

#ifndef _H_PROXYIO_I64_RB_
#define _H_PROXYIO_I64_RB_

#include <stdint.h>
#include <inttypes.h>
#include "krb.h"

struct i64_rbe {
    struct rb_node  rb;
    int64_t         key;
    void*            data;
};

struct i64_rb {
    int64_t elem_size;
    struct rb_root root;
};

#define i64_rb_init(tree)   do {        \
        INIT_RB_ROOT(&(tree)->root);    \
        (tree)->elem_size = 0;      \
    } while (0)

#define i64_rb_empty(tree) RB_EMPTY_ROOT(&(tree)->root)

/* Return the min rb_node of tree if tree is non-empty */
struct i64_rbe* i64_rb_min(struct i64_rb* tree);

/* Return the max rb_node of tree if tree is non-empty */
struct i64_rbe* i64_rb_max(struct i64_rb* tree);

/* Insert one node into rbtree */
void i64_rb_insert(struct i64_rb* tree, struct i64_rbe* node);

/* Remove node from rbtree */
void i64_rb_delete(struct i64_rb* tree, struct i64_rbe* node);

#endif /* _H_PROXYIO_I64_RB_ */
