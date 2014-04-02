#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include <inttypes.h>
#include "transport/transport.h"


#define PF_NET    TP_TCP
#define PF_IPC    TP_IPC
#define PF_INPROC TP_MOCK_INPROC

/*
  The transport protocol header is 10 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    ctlsz   | payloadlen |
  +--------+------------+------------+
*/

struct channel_msghdr {
    uint16_t checksum;
    uint32_t payload_sz;
    uint32_t control_sz;
};

struct channel_msg {
    char *payload;
    char *control;
};

uint32_t channel_msgiov_len(struct channel_msg *msg);
char *channel_msgiov_base(struct channel_msg *msg);

struct channel_msg *channel_allocmsg(uint32_t payload_sz,
    uint32_t control_sz);
void channel_freemsg(struct channel_msg *msg);


int channel_listen(int pf, const char *sock);
int channel_connect(int pf, const char *peer);
int channel_accept(int cd);
int channel_recv(int cd, struct channel_msg **msg);
int channel_send(int cd, struct channel_msg *msg);
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);
void channel_close(int cd);

#endif
