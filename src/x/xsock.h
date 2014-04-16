#ifndef _HPIO_X
#define _HPIO_X

#include <inttypes.h>
#include "transport/transport.h"


#define PF_NET    TP_TCP
#define PF_IPC    TP_IPC
#define PF_INPROC TP_MOCK_INPROC

char *xallocmsg(uint32_t size);
void xfreemsg(char *payload);
uint32_t xmsglen(char *payload);

int xlisten(int pf, const char *sock);
int xconnect(int pf, const char *peer);
int xaccept(int cd);
int xrecv(int cd, char **payload);
int xsend(int cd, char *payload);
void xclose(int cd);



#define XNOBLOCK 1
#define XSNDBUF  2
#define XRCVBUF  4

int xsetopt(int cd, int opt, void *val, int valsz);
int xgetopt(int cd, int opt, void *val, int valsz);


/* EXPORT user poll implementation, different from xpoll */

#define XPOLLIN   1
#define XPOLLOUT  2
#define XPOLLERR  4

extern const char *xpoll_str[];

struct xpoll_event {
    int cd;
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
