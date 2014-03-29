#ifndef _HPIO_CHANNEL_POLLER_
#define _HPIO_CHANNEL_POLLER_

#include <inttypes.h>

#define CHANNEL_MSGIN 1
#define CHANNEL_MSGOUT 2
#define CHANNEL_ERROR 4

#define PIO_MAX_CHANNELPOLLERS 1024

struct channel_event {
    uint32_t events;
    void *ptr;
};


int channelpoller_create();
int channelpoller_setev(int pd, int cd, struct channel_event *ev);
int channelpoller_getev(int pd, int cd, struct channel_event *ev);
int channelpoller_close(int pd);
int channelpoller_wait(int pd, struct channel_event *ready_channels,
        int max_events);








#endif
