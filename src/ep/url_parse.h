#ifndef _HPIO_URL_PARSE_
#define _HPIO_URL_PARSE_

#include <base.h>

int url_parse_group(const char *url, char *buff, u32 size);

int url_parse_pf(const char *url);

int url_parse_sockaddr(const char *url, char *buff, u32 size);

#endif
