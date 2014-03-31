#include <iostream>
#include <gtest/gtest.h>
extern "C" {
#include "bufio/bio.h"
}

extern int randstr(char *buf, int len);
char buf1[PAGE_SIZE * 4] = {}, buf2[PAGE_SIZE * 4] = {};

static void bufio_test() {
    struct bio b = {};
    int i;
    int64_t rlen = rand() % PAGE_SIZE + 3 * PAGE_SIZE;

    bio_init(&b);
    randstr(buf1, sizeof(buf1));
    for (i = 0; i < rlen; i++)
	EXPECT_EQ(1, bio_write(&b, buf1 + i, 1));
    EXPECT_EQ(b.bsize, rlen);
    EXPECT_EQ(rlen, bio_copy(&b, buf2, 4 * PAGE_SIZE));
    EXPECT_EQ(rlen, bio_read(&b, buf2, 4 * PAGE_SIZE));
    EXPECT_EQ(b.bsize, 0);
    EXPECT_TRUE(list_empty(&b.page_head));
    EXPECT_TRUE(memcmp(buf1, buf2, rlen) == 0);
    bio_destroy(&b);
}

struct test_io {
    int64_t ridx;
    int64_t widx;
    struct io io_ops;
};

static int64_t test_io_read(struct io *ops, char *buff, int64_t sz) {
    struct test_io *tio = cont_of(ops, struct test_io, io_ops);
    int64_t len = rand() % PAGE_SIZE;

    if (rand() % 2 == 0)
	return -1;
    len = len > sz ? sz : len;
    memcpy(buff, buf1 + tio->ridx, len);
    tio->ridx += len;
    return len;
}

static int64_t test_io_write(struct io *ops, char *buff, int64_t sz) {
    struct test_io *tio = cont_of(ops, struct test_io, io_ops);
    int64_t len = rand() % PAGE_SIZE;

    if (rand() % 2 == 0)
	return -1;
    len = len > sz ? sz : len;
    memcpy(buf2 + tio->widx, buff, len);
    tio->widx += len;
    return len;
}

static void bufio_fetch_flush_test() {
    struct bio b = {};
    int i;
    struct test_io tio = {};
    uint64_t nbytes = 0;
    int64_t rc, alen = 0;

    bio_init(&b);
    tio.io_ops.read = test_io_read;
    tio.io_ops.write = test_io_write;
    randstr(buf1, sizeof(buf1));
    for (i = 0; i < 4; i++) {
	if ((rc =  bio_prefetch(&b, &tio.io_ops)) > 0)
	    nbytes += rc;
    }
    alen = nbytes;
    bio_copy(&b, buf2, nbytes);
    EXPECT_TRUE(memcmp(buf1, buf2, alen) == 0);
    bio_copy(&b, buf2, nbytes);
    EXPECT_TRUE(memcmp(buf1, buf2, alen) == 0);
    EXPECT_TRUE(b.bsize == nbytes);
    while (nbytes > 0) {
	if ((rc =  bio_flush(&b, &tio.io_ops)) > 0)
	    nbytes -= rc;
    }
    EXPECT_EQ(b.bsize, 0);
    EXPECT_TRUE(list_empty(&b.page_head));
    EXPECT_TRUE(memcmp(buf1, buf2, alen) == 0);
    bio_destroy(&b);
}

TEST(bufio, bio) {
    bufio_test();
    bufio_fetch_flush_test();
}
