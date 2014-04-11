#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

struct ep;

#define PRODUCER    1
#define COMSUMER    2

struct ep *ep_new(int ty);
void ep_close(struct ep *ep);

int ep_listen(struct ep *ep, const char *url);
int ep_connect(struct ep *ep, const char *url);
char *ep_recv(struct ep *ep);
int ep_send(struct ep *ep, char *payload);

char *ep_allocmsg(int size);
void ep_freemsg(char *payload);


#endif
