#ifndef _HPIO_XCPU_
#define _HPIO_XCPU_

#include <ds/list.h>
#include <sync/mutex.h>
#include <sync/spin.h>
#include <os/eventloop.h>
#include <os/efd.h>

struct xcpu {
    //spin_t lock; // for release mode

    mutex_t lock; // for debug mode

    /* Backend eventloop for cpu_worker. */
    eloop_t el;

    ev_t efd_et;
    struct efd efd;

    /* Waiting for closed xsock will be attached here */
    struct list_head shutdown_socks;
};

int xcpu_alloc();
int xcpu_choosed(int xd);
void xcpu_free(int cpu_no);
struct xcpu *xcpuget(int cpu_no);



#endif
