#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/base.h>
#include <utils/i64_rb.h>
#include <utils/str_rb.h>
#include <utils/alloc.h>
#include <utils/conhash.h>

#define cnt 1000

static int cmpint64 (const void *p1, const void *p2)
{
	/* The actual arguments to this function are "pointers to
	   pointers to char", but strcmp(3) arguments are "pointers
	   to char", hence the following cast plus dereference */
	return * ((int64_t *) p1) - * ((int64_t *) p2);
}


static int i64_rb_test_single()
{
	int i;
	int64_t allval[cnt] = {}, *allval_ptr = &allval[0];
	int64_t min_val = 0;
	struct i64_rb tree;
	struct i64_rbe *node = NULL, *min_node = NULL;

	i64_rb_init (&tree);

	for (i = 0; i < cnt; i++) {
		BUG_ON ((node = (struct i64_rbe *) mem_zalloc (sizeof (struct i64_rbe))) == NULL);
		node->key = rand() + 1;
		if (min_val == 0 || node->key < min_val)
			min_val = node->key;
		*allval_ptr++ = node->key;
		i64_rb_insert (&tree, node);
		min_node = i64_rb_min (&tree);
		BUG_ON (min_node->key != min_val);
	}
	qsort (allval, allval_ptr - allval, sizeof (int64_t), cmpint64);
	for (i = 0; allval + i < allval_ptr; i++) {
		min_node = i64_rb_min (&tree);
		BUG_ON (min_node->key != allval[i]);
		i64_rb_delete (&tree, min_node);
		free (min_node);
	}
	return 0;
}

static void conhash_test ()
{
	struct conhash_list cl;
	void *owner;

	conhash_list_init (&cl);
	BUG_ON (conhash_list_get (&cl, "1", 1));
	conhash_list_add (&cl, "0", 1, (void *) 1);
	BUG_ON (!(owner = conhash_list_get (&cl, "1", 1)));
	conhash_list_add (&cl, "2", 1, (void *) 2);
	BUG_ON (!(owner = conhash_list_get (&cl, "1", 1)));
	BUG_ON (!conhash_list_add (&cl, "2", 1, (void *) 2));
	BUG_ON (!(owner = conhash_list_get (&cl, "4", 1)));
	BUG_ON (conhash_list_rm (&cl, "2", 1));
	BUG_ON (conhash_list_add (&cl, "2", 1, (void *) 2));
	conhash_list_rm (&cl, "0", 1);
	owner = conhash_list_get (&cl, "1", 1);
	BUG_ON (owner != (void *) 2);
	conhash_list_rm (&cl, "2", 1);
	BUG_ON (conhash_list_get (&cl, "1", 1));
	conhash_list_destroy (&cl);
}

static int map_test_single()
{
	struct str_rbe n[5];
	struct str_rb map;

	str_rb_init (&map);
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

	str_rb_insert (&map, &n[0]);
	str_rb_insert (&map, &n[1]);
	str_rb_insert (&map, &n[2]);
	str_rb_insert (&map, &n[3]);
	str_rb_insert (&map, &n[4]);

	BUG_ON (str_rb_min (&map) != &n[2]);
	BUG_ON (str_rb_max (&map) != &n[1]);

	BUG_ON (str_rb_find (&map, n[0].key, n[0].keylen) != &n[0]);
	BUG_ON (str_rb_find (&map, n[1].key, n[1].keylen) != &n[1]);
	BUG_ON (str_rb_find (&map, n[2].key, n[2].keylen) != &n[2]);
	BUG_ON (str_rb_find (&map, n[3].key, n[3].keylen) != &n[3]);
	BUG_ON (str_rb_find (&map, n[4].key, n[4].keylen) != &n[4]);

	return 0;
}


int main (int argc, char **argv)
{
	i64_rb_test_single ();
	map_test_single ();
	conhash_test ();
	return 0;
}
