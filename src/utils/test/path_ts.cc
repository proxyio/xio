#include <iostream>
#include <gtest/gtest.h>
extern "C" {
#include <utils/filepath.h>
}

using namespace std;

static int entries_cnt = 0;

static void my_walkfn(const char *path, void *data) {
    entries_cnt++;
}


static int path_func_test() {
    char *p;
    EXPECT_TRUE(string(p = fp_dir("")) == "."); free(p);
    EXPECT_TRUE(string(p = fp_base("")) == "."); free(p);
    EXPECT_TRUE(string(p = fp_dir("gtest.h")) == "."); free(p);
    EXPECT_TRUE(string(p = fp_base("gtest.h")) == "gtest.h"); free(p);
    EXPECT_TRUE(string(p = fp_dir("./gtest.h")) == "."); free(p);
    EXPECT_TRUE(string(p = fp_base("./gtest.h")) == "gtest.h"); free(p);
    EXPECT_TRUE(string(p = fp_dir("/tmp/gtest/gtest.h")) == "/tmp/gtest"); free(p);
    EXPECT_TRUE(string(p = fp_base("/tmp/gtest/gtest.h")) == "gtest.h"); free(p);
    EXPECT_TRUE(string(p = fp_abs("/tmp/gtest/gtest.h")) == "/tmp/gtest/gtest.h"); free(p);
    EXPECT_TRUE(fp_hasprefix("/tmp/gtest", "/tmp"));
    EXPECT_TRUE(fp_hasprefix("/tmp/gtest", ""));
    EXPECT_TRUE(fp_hasprefix("/tmp/gtest", "/tmp/gtest"));
    EXPECT_TRUE(!fp_hasprefix("/tmp/gtest", "/tmp/gtest/"));
    EXPECT_TRUE(fp_hassuffix("/tmp/gtest", "gtest"));
    EXPECT_TRUE(fp_hassuffix("/tmp/gtest", ""));
    EXPECT_TRUE(fp_hassuffix("/tmp/gtest", "/tmp/gtest"));
    EXPECT_TRUE(!fp_hassuffix("/tmp/gtest", "/tmp/gtest/"));
    return 0;
}

static int path_walk_test() {
    filepath_t fp = {};

    filepath_init(&fp, "");
    fp_walkdir(&fp, my_walkfn, NULL);
    fp_dwalkdir(&fp, my_walkfn, NULL, -1);
    EXPECT_TRUE(entries_cnt != 0);
    filepath_destroy(&fp);
    return 0;
}


TEST(path, walk) {
    path_walk_test();
    path_func_test();
}
