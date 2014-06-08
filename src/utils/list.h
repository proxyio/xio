/*
  I grub it from linux kernel source code and fix it into user
  space program and remove some compiler specified usage. such
  as typeof()/prefetch()... and so on.

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

#ifndef _XIO_LIST_
#define _XIO_LIST_

#include "base.h"

//
// These are non-NULL pointers that will result in page faults
// under normal circumstances, used to verify that nobody uses
// non-initialized list entries.
//
#define LIST_POISON1  ((struct list_head *) 0x00100100)
#define LIST_POISON2  ((struct list_head *) 0x00200200)



// Simple doubly linked list implementation
//
// Some of the internal functions ("__xxx") are usefull when manipulating
// whole lists rather than single entries, as sometimes we already know the
// next/prev entries and we can generate better code by using them directly
// rather than using the generic single-entry routines.

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INITIALIZE(name) { &(name), &(name) }
#define LIST_ITEM_INITIALIZE { NULL, NULL }
#define LIST_HEAD(name)					\
    struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do {			\
	(ptr)->next = (ptr); (ptr)->prev = (ptr);	\
    } while(0)


struct list_link {
	void *self;
	int linked;
	struct list_head node;
};

#define INIT_LIST_LINK(link) do {		\
	memset((link), 0, sizeof(*(link)));	\
	(link)->self = this;			\
	(link)->linked = 0;			\
    } while (0)

#define walk_each_list_link(pos, head)				\
    walk_each_entry((pos), (head), struct list_link, node)
#define walk_each_list_link_s(pos, next, head)				\
    walk_each_entry_s((pos), (next), (head), struct list_link, node)


#define list_first(head, type, member)		\
    cont_of((head)->next, type, member)

#define list_last(head, type, member)		\
    cont_of((head)->prev, type, member)


// Insert a new entry between two known consecutive entries
// this is only for internal list manipulation where we know
// the prev/next entries already!

static inline void __list_add(struct list_head *node,
                              struct list_head *prev,
                              struct list_head *next)
{
	// link to next
	next->prev = node;
	node->next = next;

	// link to prev
	node->prev = prev;
	prev->next = node;
}

// Insert a new entry after the specified head. this is good for
// implementing stacks

static inline void list_add(struct list_head *node, struct list_head *head)
{
	__list_add(node, head, head->next);
}


// Insert a new entry before the specified head. this is good for
// implementing stacks
static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
	__list_add(node, head->prev, head);
}


// Delete a list entry by marking the prev/next entries point to
// each other
// This is a only ofr internal list manipulation where we know the
// prev/next entries already!

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}


// Deletes entry from list
// Note: list_empty on entry does not return true after this, the entry
// is in an undefined state.
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}


// Deletes entry from list and reinitialize it
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}


// Delete from one list and add as another's head
static inline void list_move(struct list_head *entry, struct list_head *head)
{
	__list_del(entry->prev, entry->next);
	list_add(entry, head);
}


// Delete from one list and add as another's tail
static inline void list_move_tail(struct list_head *entry, struct list_head *head)
{
	__list_del(entry->prev, entry->next);
	list_add_tail(entry, head);
}


// Tests whether a list is empty
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define attached(head) !list_empty(head)

// Join two lists
// all entries in head1 will be moved into head2's head
static inline void __list_splice(struct list_head *head1, struct list_head *head2)
{
	struct list_head *first = head1->next;
	struct list_head *last = head1->prev;
	struct list_head *at = head2->next;

	first->prev = head2;
	head2->next = first;

	last->next = at;
	at->prev = last;
}

// Join two lists and then reinitialize the first head
static inline void list_splice(struct list_head *head1, struct list_head *head2)
{
	if (!list_empty(head1)) {
		__list_splice(head1, head2);
		INIT_LIST_HEAD(head1);
	}
}



// Get the struct for this entry
// @param ptr         the &struct list_head pointer
// @param type        the type of the struct this is embedded in.
// @param member      the name of the list_struct within the sturct.
#define list_entry(ptr, type, member) cont_of(ptr, type, member)

#define list_next(pos) (pos)->next

// Interate over a list
// @param pos        the &struct list_head to use as a loop conter.
// @param head       the head for your list.
#define walk_each(pos, head)					\
    for (pos = (head)->next; pos != head; pos = (pos)->next)


// Interate over a list backwards
// @param pos        the &struct list_head to use as a loop conter.
// @param head       the head for your list.
#define walk_each_prev(pos, head)				\
    for (pos = (head)->prev; pos != head; pos = (pos)->prev)



// Interate over a list safe against removal of list entry
// @param pos       the &struct list_head to use as a loop conter.
// @param n         another &struct list_head to use as temp storage
// @param head      the head for your list.
#define walk_each_s(pos, n, head)				\
    for (pos = (head)->next, n = pos->next; pos != head;	\
	 pos = n; n = pos->next)


// Interate over list of a given type
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry(pos, head, type, member)		\
    for (pos = list_entry((head)->next, type, member);		\
	 &(pos)->member != head;				\
	 pos = list_entry((pos)->member.next, type, member))


// Interate over a list backwards of a given type
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry_prev(pos, head, type, member)		\
    for (pos = list_entry((head)->prev, type, member);		\
	 &(pos)->member != head;				\
	 pos = list_entry((pos)->member.prev, type, member))



// Interate over a list of given type continuing after existing point
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry_continue(pos, head, type, member)	\
    for (pos = list_entry((pos)->member.next, type, member);	\
	 &(pos)->member != head;				\
	 pos = list_entry((pos)->member.next, type, member))



// Interate over a list of given type safe against removal of list
// entry
//
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry_s(pos, n, head, type, member)		\
    for (pos = list_entry((head)->next, type, member),		\
	     n = list_entry(pos->member.next, type, member);	\
	 &pos->member != head;					\
	 pos = n, n = list_entry(n->member.next, type, member))


// Interate over a list backwards of given type safe against removal
// of list entry
//
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry_prev_s(pos, n, head, type, member)	\
    for (pos = list_entry((head)->prev, type, member),		\
	     n = list_entry(pos->member.prev, type, member);	\
	 &pos->member != head;					\
	 pos = n, n = list_entry(n->member.prev, type, member))


// Interate over a list of given type continuing after existing
// point safe against removal of list entry
//
// @param pos       the type * to use as a loop counter.
// @param head      the head for your list.
// @param type      the list_head struct container type
// @param member    the field name of the list_head struct within..
#define walk_each_entry_continue_s(pos, n, head, type, member)	\
    for (pos = list_entry(pos->member.next, type, member),	\
	     n = list_entry(pos->member.next, type, member);	\
	 &pos->member != head;					\
	 pos = n, n = list_entry(n->member.next, type, member))






#define HLIST_POISON1  ((struct hlist_node *) 0x00100100)
#define HLIST_POISON2  ((struct hlist_node **) 0x00200200)


// Double linked lists with a single pointer list head
// Mostly useful for hash table whare the two pointer list head is
// too wasteful. you lose the ability to access the tail in O(1).

struct hlist_head {
	struct hlist_node *first;
};


struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define HLIST_HEAD_INIT { .first = 0 }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = 0)
#define INIT_HLIST_NODE(ptr) do {		\
	(ptr)->next = 0;			\
	(ptr)->pprev = 0;			\
    } while(0)

static inline int hlist_unhashed(const struct hlist_node *node)
{
	return !node->pprev;
}


static inline int hlist_empty(const struct hlist_head *head)
{
	return !head->first;
}

static inline void __hlist_del(struct hlist_node *node)
{
	struct hlist_node *next = node->next;
	struct hlist_node **pprev = node->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *node)
{
	__hlist_del(node);
	node->next = HLIST_POISON1;
	node->pprev = HLIST_POISON2;
}



static inline void hlist_del_init(struct hlist_node *node)
{
	if (node->pprev) {
		__hlist_del(node);
		INIT_HLIST_NODE(node);
	}
}


static inline void hlist_add_head(struct hlist_node *node, struct hlist_head *head)
{
	struct hlist_node *first = head->first;
	node->next = first;
	if (first)
		first->pprev = &node->next;
	head->first = node;
	node->pprev = &head->first;
}


// next must be != NULL
static inline void hlist_add_before(struct hlist_node *node, struct hlist_node *next)
{
	node->pprev = next->pprev;
	node->next = next;
	next->pprev = &node->next;
	*(node->pprev) = node;
}


static inline void hlist_add_after(struct hlist_node *node, struct hlist_node *prev)
{
	node->next = prev->next;
	if (node->next)
		node->next->pprev = &node->next;
	node->pprev = &prev->next;
	*(node->pprev) = node;
}


#define hlist_entry(ptr, type, member) cont_of(ptr, type, member)

#define hwalk_each(pos, head)				\
    for (pos = (head)->first; pos; pos = pos->next)

#define hwalk_each_s(pos, n, head)					\
    for (pos = (head)->first; pos && ({n = pos->next; 1;}); pos = n)

//
// hwalk_each_entry	- iterate over list of given type
// @param pos	the &struct hlist_node to use as a loop counter.
// @param head	the head for your list.
// @param type
// @param member        the name of the hlist_node within the struct.

#define hwalk_each_entry(pos, head, type, member)			\
    for (pos = (type *)(head)->first; pos &&				\
	     ({pos = hlist_entry((struct hlist_node *)pos, type, member); 1;}); \
	 pos = (type *)pos->member.next)


// hwalk_each_entry_safe - iterate over list of given type safe against
// removal of list entry
//
// @param pos	the &struct hlist_node to use as a loop counter.
// @param n	another &struct hlist_node to use as temporary storage
// @param head	the head for your list.
// @param type
// @param member        the name of the hlist_node within the struct.


#define hwalk_each_entry_s(pos, n, head, type, member)			\
    for (pos = (type *)(head)->first;					\
	 pos && ({pos = hlist_entry((struct hlist_node *)pos, type, member); \
		 n = pos->member.next ? hlist_entry(pos->member.next, type, member) : NULL; 1;}); \
	 pos = n ? (type *)&n->member : NULL)


// hwalk_each_entry_continue - iterate over a hlist continuing
// after existing point
//
// @param pos	the &struct hlist_node to use as a loop counter.
// @param type
// @param member	the name of the hlist_node within the struct.

#define hwalk_each_entry_continue(pos, type, member)			\
    for (pos = (type *)(pos)->member.next; pos &&			\
	     ({ pos = hlist_entry((struct hlist_node *)pos, type, member); 1;}); \
	 pos = (type *)pos->member.next)

// hwalk_each_entry_from - iterate over a hlist continuing
// from existing point
//
// @param pos	the &struct hlist_node to use as a loop counter.
// @param type
// @param member	the name of the hlist_node within the struct.

#define hwalk_each_entry_from(pos, type, member)			\
    for (pos = (type *)&pos->member; pos &&				\
	     ({pos = hlist_entry((struct hlist_node *)pos, type, member); 1;}); \
	 pos = (type *)pos->member.next)


#endif // _LIST_H_
