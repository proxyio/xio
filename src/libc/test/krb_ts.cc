#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <algorithm>
extern "C" {
#include "ds/skrb.h"
#include "ds/map.h"
#include "os/alloc.h"
}

using namespace std;

static int skrb_test_single() {
    int i, cnt = 1000;
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


static int map_test_single() {
    struct ssmap_node n[5];
    ssmap_t map;
    string key;

    ssmap_init(&map);
    n[0].key = "11111";
    n[0].keylen = strlen(n[0].key);
    n[1].key = "11112";
    n[1].keylen = strlen(n[1].key);
    n[2].key = "10112";
    n[2].keylen = strlen(n[2].key);
    n[3].key = "101121";
    n[3].keylen = strlen(n[3].key);
    n[4].key = "1111";
    n[4].keylen = strlen(n[4].key);

    ssmap_insert(&map, &n[0]);
    ssmap_insert(&map, &n[1]);
    ssmap_insert(&map, &n[2]);
    ssmap_insert(&map, &n[3]);
    ssmap_insert(&map, &n[4]);

    EXPECT_TRUE(ssmap_min(&map) == &n[2]);
    EXPECT_TRUE(ssmap_max(&map) == &n[1]);

    EXPECT_TRUE(ssmap_find(&map, n[0].key, n[0].keylen) == &n[0]);
    EXPECT_TRUE(ssmap_find(&map, n[1].key, n[1].keylen) == &n[1]);    
    EXPECT_TRUE(ssmap_find(&map, n[2].key, n[2].keylen) == &n[2]);
    EXPECT_TRUE(ssmap_find(&map, n[3].key, n[3].keylen) == &n[3]);
    EXPECT_TRUE(ssmap_find(&map, n[4].key, n[4].keylen) == &n[4]);

    return 0;
}


TEST(skrb, single) {
    skrb_test_single();
    map_test_single();
}
