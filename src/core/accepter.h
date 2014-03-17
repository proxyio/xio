#ifndef _HPIO_ACCEPTER_
#define _HPIO_ACCEPTER_

#include "core.h"



void acp_init(acp_t *acp, const struct cf *cf);
void acp_destroy(acp_t *acp);
void acp_start(acp_t *acp);
void acp_stop(acp_t *acp);
int acp_listen(acp_t *acp, const char *addr);

proxy_t *acp_getpy(acp_t *acp, const char *pyn);
int acp_proxyto(acp_t *acp, const char *pyn, const char *addr);



#endif
