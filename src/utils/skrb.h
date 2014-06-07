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

#ifndef _XIO_SKRB_
#define _XIO_SKRB_

#include <stdint.h>
#include <inttypes.h>
#include "krb.h"

typedef struct skrb_node {
    struct rb_node  rb;
    int64_t         key;
    void            *data;
} skrb_node_t;

typedef struct skrb {
    int64_t elem_size;
    struct rb_root root;
} skrb_t;

#define skrb_init(tree)	do {			\
	INIT_RB_ROOT(&(tree)->root);		\
	(tree)->elem_size = 0;			\
    } while (0)

#define skrb_empty(tree) RB_EMPTY_ROOT(&(tree)->root)


static inline skrb_node_t *skrb_min(skrb_t *tree)
{
    struct rb_root *root = &tree->root;
    struct rb_node *cur = root->rb_node, *parent = NULL;
    skrb_node_t *_min = NULL;

    while (cur) {
        parent = cur;
        cur = parent->rb_left;
        _min = rb_entry(parent, skrb_node_t, rb);
    }
    return _min;
}


static inline skrb_node_t *skrb_max(skrb_t *tree)
{
    struct rb_root *root = &tree->root;
    struct rb_node *cur = root->rb_node, *parent = NULL;
    skrb_node_t *_max = NULL;

    while (cur) {
        parent = cur;
        cur = parent->rb_right;
        _max = rb_entry(parent, skrb_node_t, rb);
    }
    return _max;
}

static inline void skrb_insert(skrb_t *tree, skrb_node_t *node)
{
    struct rb_root *root = &tree->root;
    struct rb_node **np = &root->rb_node, *parent = NULL;
    int64_t key = node->key;

    tree->elem_size++;
    while (*np) {
        parent = *np;
        if (key < rb_entry(parent, skrb_node_t, rb)->key)
            np = &parent->rb_left;
        else
            np = &parent->rb_right;
    }

    rb_link_node(&node->rb, parent, np);
    rb_insert_color(&node->rb, root);
}


static inline void skrb_delete(skrb_t *tree, skrb_node_t *node)
{
    tree->elem_size--;
    rb_erase(&node->rb, &tree->root);
}

#endif /* _H_SKRB_ */
