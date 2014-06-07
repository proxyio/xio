#include <utils/filepath.h>

static int entries_cnt = 0;

static void my_walkfn(const char *path, void *data)
{
    entries_cnt++;
}

static void test_ffp(char *ptr, const char *dst)
{
    BUG_ON(strcmp(ptr, dst) != 0);
    free(ptr);
}

static void test_fp(const char *ptr, const char *dst)
{
    BUG_ON(strcmp(ptr, dst) != 0);
}


static int path_func_test()
{
    char *p;
    test_ffp(fp_dir(""), ".");
    test_ffp(fp_dir("gtest.h"), ".");
    test_ffp(fp_base("gtest.h"), "gtest.h");
    test_ffp(fp_dir("./gtest.h"), ".");
    test_ffp(fp_base("./gtest.h"), "gtest.h");
    test_ffp(fp_dir("/tmp/gtest/gtest.h"), "/tmp/gtest");
    test_ffp(fp_base("/tmp/gtest/gtest.h"), "gtest.h");
    test_ffp(fp_abs("/tmp/gtest/gtest.h"), "/tmp/gtest/gtest.h");
    BUG_ON(!fp_hasprefix("/tmp/gtest", "/tmp"));
    BUG_ON(!fp_hasprefix("/tmp/gtest", ""));
    BUG_ON(!fp_hasprefix("/tmp/gtest", "/tmp/gtest"));
    BUG_ON(fp_hasprefix("/tmp/gtest", "/tmp/gtest/"));
    BUG_ON(!fp_hassuffix("/tmp/gtest", "gtest"));
    BUG_ON(!fp_hassuffix("/tmp/gtest", ""));
    BUG_ON(!fp_hassuffix("/tmp/gtest", "/tmp/gtest"));
    BUG_ON(fp_hassuffix("/tmp/gtest", "/tmp/gtest/"));
    return 0;
}

static int path_walk_test()
{
    filepath_t fp = {};

    filepath_init(&fp, "");
    fp_walkdir(&fp, my_walkfn, NULL);
    fp_dwalkdir(&fp, my_walkfn, NULL, -1);
    BUG_ON(entries_cnt == 0);
    filepath_destroy(&fp);
    return 0;
}

int main(int argc, char **argv)
{
    path_walk_test();
    path_func_test();
    return 0;
}
