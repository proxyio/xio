#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/base.h>
#include <utils/skrb.h>
#include <utils/rb_str.h>
#include <utils/alloc.h>

#define cnt 1000

static int cmpint64 (const void *p1, const void *p2)
{
	/* The actual arguments to this function are "pointers to
	   pointers to char", but strcmp(3) arguments are "pointers
	   to char", hence the following cast plus dereference */
	return * ( (int64_t *) p1) - * ( (int64_t *) p2);
}


static int skrb_test_single()
{
	int i;
	int64_t allval[cnt] = {}, *allval_ptr = &allval[0];
	int64_t min_val = 0;
	skrb_t tree;
	skrb_node_t *node = NULL, *min_node = NULL;

	skrb_init (&tree);

	for (i = 0; i < cnt; i++) {
		BUG_ON ( (node = (skrb_node_t *) mem_zalloc (sizeof (skrb_node_t) ) ) == NULL);
		node->key = rand() + 1;
		if (min_val == 0 || node->key < min_val)
			min_val = node->key;
		*allval_ptr++ = node->key;
		skrb_insert (&tree, node);
		min_node = skrb_min (&tree);
		BUG_ON (min_node->key != min_val);
	}
	qsort (allval, allval_ptr - allval, sizeof (int64_t), cmpint64);
	for (i = 0; allval + i < allval_ptr; i++) {
		min_node = skrb_min (&tree);
		BUG_ON (min_node->key != allval[i]);
		skrb_delete (&tree, min_node);
		free (min_node);
	}
	return 0;
}


static int map_test_single()
{
	struct rb_str_node n[5];
	struct rb_str map;

	rb_str_init (&map);
	n[0].key = "11111";
	n[0].keylen = strlen (n[0].key);
	n[1].key = "11112";
	n[1].keylen = strlen (n[1].key);
	n[2].key = "10112";
	n[2].keylen = strlen (n[2].key);
	n[3].key = "101121";
	n[3].keylen = strlen (n[3].key);
	n[4].key = "1111";
	n[4].keylen = strlen (n[4].key);

	rb_str_insert (&map, &n[0]);
	rb_str_insert (&map, &n[1]);
	rb_str_insert (&map, &n[2]);
	rb_str_insert (&map, &n[3]);
	rb_str_insert (&map, &n[4]);

	BUG_ON (rb_str_min (&map) != &n[2]);
	BUG_ON (rb_str_max (&map) != &n[1]);

	BUG_ON (rb_str_find (&map, n[0].key, n[0].keylen) != &n[0]);
	BUG_ON (rb_str_find (&map, n[1].key, n[1].keylen) != &n[1]);
	BUG_ON (rb_str_find (&map, n[2].key, n[2].keylen) != &n[2]);
	BUG_ON (rb_str_find (&map, n[3].key, n[3].keylen) != &n[3]);
	BUG_ON (rb_str_find (&map, n[4].key, n[4].keylen) != &n[4]);

	return 0;
}


int main (int argc, char **argv)
{
	skrb_test_single();
	map_test_single();
	return 0;
}
