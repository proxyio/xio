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
    char* ubuf;

    if (argc < 2) {
        printf("usage: lat_recver <bind-to>\n");
        return 0;
    }

    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REP)) < 0);
    BUG_ON(sp_listen(eid, argv[1]) < 0);

    while (1) {
        BUG_ON(sp_recv(eid, &ubuf) != 0);
        BUG_ON(sp_send(eid, ubuf));
    }

    sp_close(eid);
    return 0;
}
