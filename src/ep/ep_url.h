#ifndef _HPIO_EPURL_
#define _HPIO_EPURL_

#include <base.h>
#include "ep_fd.h"

struct fd *fd_open(const char *url);

struct fd *fd_listen(const char *url);

struct fd *fd_accept(struct fd *f);

#endif
