#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <algorithm>
extern "C" {
#include "os/malloc.h"
#include "ds/skrb.h"
}

using namespace std;

static int skrb_test_single() {
    int i, cnt = 10000;
    std::vector<int64_t> allval;
    int64_t min_val = 0;
    skrb_t tree;
    skrb_node_t *node = NULL, *min_node = NULL;

    skrb_init(&tree);

    for (i = 0; i < cnt; i++) {
	EXPECT_TRUE((node = (skrb_node_t *)mem_zalloc(sizeof(skrb_node_t))) != NULL);
	node->key = rand() + 1;
	if (min_val == 0 || node->key < min_val)
	    min_val = node->key;
	allval.push_back(node->key);
	skrb_insert(&tree, node);
	min_node = skrb_min(&tree);
	EXPECT_TRUE(min_node->key == min_val);
    }

    std::sort(allval.begin(), allval.end());
    for (std::vector<int64_t >::iterator it = allval.begin(); it != allval.end(); ++it) {
	min_node = skrb_min(&tree);
	EXPECT_TRUE(min_node->key == *it);
	skrb_delete(&tree, min_node);
	free(min_node);
    }
    return 0;
}

TEST(skrb, single) {
    EXPECT_EQ(0, skrb_test_single());
}
