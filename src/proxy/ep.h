#ifndef _HPIO_ENDPOINT_
#define _HPIO_ENDPOINT_

struct ep;

#define PRODUCER    1
#define COMSUMER    2

extern const char *ep_tystr[3];

struct ep *ep_new(int ty);
void ep_close(struct ep *ep);

int ep_listen(struct ep *ep, const char *url);
int ep_connect(struct ep *ep, const char *url);

/* Producer endpoint api : send_req and recv_resp */
int ep_send_req(struct ep *ep, char *req);
int ep_recv_resp(struct ep *ep, char **resp);

/* Comsumer endpoint api : recv_req and send_resp */
int ep_recv_req(struct ep *ep, char **req, char **r);
int ep_send_resp(struct ep *ep, char *resp, char *r);

char *ep_allocmsg(int size);
void ep_freemsg(char *msg);


#endif
