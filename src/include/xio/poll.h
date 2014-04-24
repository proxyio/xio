#ifndef _HPIO_POLL_
#define _HPIO_POLL_

#define XPOLLIN   1
#define XPOLLOUT  2
#define XPOLLERR  4

struct xpoll_event {
    int xd;
    void *self;

    /* What events i care about ... */
    uint32_t care;

    /* What events happened now */
    uint32_t happened;
};

struct xpoll_t;

struct xpoll_t *xpoll_create();
void xpoll_close(struct xpoll_t *po);

#define XPOLL_ADD 1
#define XPOLL_DEL 2
#define XPOLL_MOD 3
int xpoll_ctl(struct xpoll_t *xp, int op, struct xpoll_event *ue);

int xpoll_wait(struct xpoll_t *xp, struct xpoll_event *events, int n,
	       u32 timeout);


#endif
