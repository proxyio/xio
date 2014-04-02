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
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);
void channel_close(int cd);

#endif
