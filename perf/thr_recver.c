#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/spinlock.h>
#include <utils/timer.h>
#include <utils/thread.h>

int main(int argc, char** argv) {
    int i;
    int eid;
    int count;
    int size;
    char* ubuf;
    int thr;
    double mbs;
    uint64_t st, lt;

    if (argc < 3) {
        printf("usage: thr_recver <bind-to> <msg-count>\n");
        return 0;
    }

    count = atoi(argv[2]);
    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REP)) < 0);
    BUG_ON(sp_listen(eid, argv[1]) < 0);

    BUG_ON(sp_recv(eid, &ubuf) != 0);
    size = 0;
    st = gettimeofms();

    while (count > 0) {
        BUG_ON(sp_recv(eid, &ubuf) != 0);
        count--;
        size += usize(ubuf);

        if (count) {
            ufree(ubuf);
        } else {
            BUG_ON(sp_send(eid, ubuf) != 0);
        }
    }

    lt = gettimeofms();

    thr = atoi(argv[2]) * 1000 / (lt - st);
    mbs = (double)(size * 8 / (lt - st)) / 1024;

    printf("message size: %d [B]\n", size);
    printf("throughput: %d [msg/s]\n", thr);
    printf("throughput: %.3f [Mb/s]\n", mbs);

    sp_close(eid);
    return 0;
}
