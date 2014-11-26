/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>
  (C) 2012  Michel Lespinasse <walken@google.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree_augmented.h

  I grub it from linux kernel source code and fix it into user space program
  and remove some compiler specified usage. such as __always_inline ... and so on.
                       -----------   2014/02/09 Dong Fang <yp.fangdong@gmail.com>
*/

#include "krb_augmented.h"

struct rb_node*
__rb_erase_augmented(struct rb_node* node, struct rb_root* root,
                     const struct rb_augment_callbacks* augment) {
    struct rb_node* child = node->rb_right, *tmp = node->rb_left;
    struct rb_node* parent, *rebalance;
    unsigned long pc;

    if (!tmp) {
        /*
         * Case 1: node to erase has no more than 1 child (easy!)
         *
         * Note that if there is one child it must be red due to 5)
         * and node must be black due to 4). We adjust colors locally
         * so as to bypass __rb_erase_color() later on.
         */
        pc = node->__rb_parent_color;
        parent = __rb_parent(pc);
        __rb_change_child(node, child, parent, root);

        if (child) {
            child->__rb_parent_color = pc;
            rebalance = NULL;
        } else {
            rebalance = __rb_is_black(pc) ? parent : NULL;
        }

        tmp = parent;
    } else if (!child) {
        /* Still case 1, but this time the child is node->rb_left */
        tmp->__rb_parent_color = pc = node->__rb_parent_color;
        parent = __rb_parent(pc);
        __rb_change_child(node, tmp, parent, root);
        rebalance = NULL;
        tmp = parent;
    } else {
        struct rb_node* successor = child, *child2;
        tmp = child->rb_left;

        if (!tmp) {
            /*
             * Case 2: node's successor is its right child
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (s)  ->  (x) (c)
             *        \
             *        (c)
             */
            parent = successor;
            child2 = successor->rb_right;
            augment->copy(node, successor);
        } else {
            /*
             * Case 3: node's successor is leftmost under
             * node's right child subtree
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (y)  ->  (x) (y)
             *      /            /
             *    (p)          (p)
             *    /            /
             *  (s)          (c)
             *    \
             *    (c)
             */
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);

            parent->rb_left = child2 = successor->rb_right;
            successor->rb_right = child;
            rb_set_parent(child, successor);
            augment->copy(node, successor);
            augment->propagate(parent, successor);
        }

        successor->rb_left = tmp = node->rb_left;
        rb_set_parent(tmp, successor);

        pc = node->__rb_parent_color;
        tmp = __rb_parent(pc);
        __rb_change_child(node, successor, tmp, root);

        if (child2) {
            successor->__rb_parent_color = pc;
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = NULL;
        } else {
            unsigned long pc2 = successor->__rb_parent_color;
            successor->__rb_parent_color = pc;
            rebalance = __rb_is_black(pc2) ? parent : NULL;
        }

        tmp = successor;
    }

    augment->propagate(tmp, NULL);
    return rebalance;
}

void rb_erase_augmented(struct rb_node* node, struct rb_root* root,
                        const struct rb_augment_callbacks* augment) {
    struct rb_node* rebalance = __rb_erase_augmented(node, root, augment);

    if (rebalance) {
        __rb_erase_color(rebalance, root, augment->rotate);
    }
}
