#ifndef _HPIO_ACCEPTER_
#define _HPIO_ACCEPTER_

#include "core.h"



void acp_init(acp_t *acp, int w);
void acp_destroy(acp_t *acp);
proxy_t *acp_find(acp_t *acp, char proxyname[PROXYNAME_MAX]);
int acp_worker(void *args);
void acp_start(acp_t *acp);
void acp_stop(acp_t *acp);
int acp_listen(acp_t *acp, const char *addr);
int acp_proxyto(acp_t *acp, char *proxyname, const char *addr);



#endif
