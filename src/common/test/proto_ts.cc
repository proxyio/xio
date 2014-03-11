#include <gtest/gtest.h>
#include <errno.h>
extern "C" {
#include "os/timestamp.h"
#include "proto/parser.h"
}

using namespace std;

static char buf[1024] = {};

typedef struct testconn {
    io_t c;
    char buf[2048];
    int send_idx, recv_idx;
} testconn_t;

int64_t test_read(io_t *c, char *buf, int64_t size) {
    testconn_t *tc = container_of(c, testconn_t, c);
    if (rand() % 2 == 0) {
	errno = EAGAIN;
	return -1;
    }
    memcpy(buf, tc->buf + tc->recv_idx, 1);
    tc->recv_idx++;
    return 1;
}

int64_t test_write(io_t *c, char *buf, int64_t size) {
    testconn_t *tc = container_of(c, testconn_t, c);
    if (rand() % 2 == 0) {
	errno = EAGAIN;
	return -1;
    }
    memcpy(tc->buf + tc->send_idx, buf, 1);
    tc->send_idx++;
    return 1;
}

void tc_init(testconn_t *tc) {
    ZERO(*tc);
    tc->c.read = test_read;
    tc->c.write = test_write;
}


void proto_parser_test() {
    struct pio_parser pp = {};
    testconn_t tc = {};
    int nbytes;
    struct pio_rt rt = {};
    pio_msg_t *msg = NULL, msg2 = {};

    msg2.hdr.version = 1;
    msg2.hdr.ttl = 1;
    msg2.hdr.end_ttl = 1;
    msg2.hdr.size = 1024;
    msg2.hdr.go = 1;
    msg2.hdr.seqid = 1;
    msg2.hdr.sendstamp = rt_mstime();
    ph_makechksum(&msg2.hdr);
    msg2.data = buf;
    msg2.rt = &rt;
    
    tc_init(&tc);
    pp_init(&pp, 0);
    while ((nbytes = pp_send(&pp, &tc.c, &msg2)) < 0 && errno == EAGAIN) {
    }
    ASSERT_EQ(nbytes, 0);
    while ((nbytes = pp_recv(&pp, &tc.c, &msg, PIORTLEN)) < 0 && errno == EAGAIN) {
    }
    ASSERT_EQ(0, memcmp(&msg->hdr, &msg2.hdr, PIOHDRLEN));
    ASSERT_EQ(nbytes, 0);
    pp_destroy(&pp);
    free(msg);
}

TEST(proto, parser) {
    proto_parser_test();
}
