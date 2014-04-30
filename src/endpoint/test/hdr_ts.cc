#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
#include <endpoint/ep_hdr.h>
}

struct ts_hdr {
    struct ephdr h;
    struct epr rt[4];
    struct uhdr uh;
    char ubuf[128];
};

TEST(endpoint, hdr) {
    struct ts_hdr th;
    th.h.ttl = 4;
    th.h.go = 1;
    th.h.size = sizeof(th.ubuf);
    th.uh.ephdr_off = sizeof(th.h) + sizeof(th.rt) + sizeof(th.uh);

    BUG_ON(ephdr_rtlen(&th.h) != sizeof(th.rt));
    BUG_ON(ephdr_ctlen(&th.h) != sizeof(th.h) + sizeof(th.rt) + sizeof(th.uh));
    BUG_ON(ephdr_dlen(&th.h) != sizeof(th.ubuf));
    BUG_ON(ubuf2ephdr(th.ubuf) != &th.h);
    BUG_ON(ephdr2ubuf(&th.h) != th.ubuf);
    BUG_ON(ephdr_uhdr(&th.h) != &th.uh);
}
