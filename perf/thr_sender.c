#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/timer.h>
#include <utils/spinlock.h>
#include <utils/thread.h>

static int argc;
static char** argv;

int task_main(void* args) {
    int i;
    int eid;
    int size;
    int rts;
    char* ubuf;

    if (argc < 4) {
        printf("usage: thr_sender <connect-to> <msg-size> <roundtrips>\n");
        return 0;
    }

    size = atoi(argv[2]);
    rts = atoi(argv[3]);
    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REQ)) < 0);
    BUG_ON(sp_connect(eid, argv[1]) < 0);

    BUG_ON(sp_send(eid, ualloc(size)) != 0);

    for (i = 0; i < rts; i++) {
        BUG_ON(sp_send(eid, ualloc(size)) != 0);
    }

    BUG_ON(sp_recv(eid, &ubuf) != 0);
    ufree(ubuf);

    sp_close(eid);
    return 0;
}

int main(int _argc, char** _argv) {
    int i;
    thread_t childs[1];

    argc = _argc;
    argv = _argv;

    for (i = 0; i < NELEM(childs); i++) {
        thread_start(&childs[i], task_main, 0);
    }

    for (i = 0; i < NELEM(childs); i++) {
        thread_stop(&childs[i]);
    }

    return 0;
}
