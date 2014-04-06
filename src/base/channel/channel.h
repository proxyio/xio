#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include <inttypes.h>
#include "transport/transport.h"


#define PF_NET    TP_TCP
#define PF_IPC    TP_IPC
#define PF_INPROC TP_MOCK_INPROC


char *channel_allocmsg(uint32_t size);
void channel_freemsg(char *payload);

int channel_listen(int pf, const char *sock);
int channel_connect(int pf, const char *peer);
int channel_accept(int cd);
int channel_recv(int cd, char **payload);
int channel_send(int cd, char *payload);
void channel_close(int cd);


#define CHANNEL_POLL   1
#define CHANNEL_SNDBUF 2
#define CHANNEL_RCVBUF 4
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);


struct channel_events;
typedef void (*channel_event_func) (int cd, struct channel_events *ev);

struct channel_events {
    int events;
    channel_event_func f;
    void *self;
};

int channel_poll(int cd, struct channel_events *ev);


#endif
