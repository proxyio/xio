/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"

typedef int (*sock_setopt) (struct sockbase *self, void *val, int vallen);

static int set_noblock(struct sockbase *self, void *val, int vallen) {
    mutex_lock(&self->lock);
    self->fasync = *(int *)val ? true : false;
    mutex_unlock(&self->lock);
    return 0;
}

static int set_sndwin(struct sockbase *self, void *val, int vallen) {
    mutex_lock(&self->lock);
    self->snd.wnd = (*(int *)val);
    mutex_unlock(&self->lock);
    return 0;
}

static int set_rcvwin(struct sockbase *self, void *val, int vallen) {
    mutex_lock(&self->lock);
    self->rcv.wnd = (*(int *)val);
    mutex_unlock(&self->lock);
    return 0;
}

static int set_linger(struct sockbase *self, void *val, int vallen) {
    return -1;
}

static int set_sndtimeo(struct sockbase *self, void *val, int vallen) {
    return -1;
}

static int set_rcvtimeo(struct sockbase *self, void *val, int vallen) {
    return -1;
}

static int set_reconnect(struct sockbase *self, void *val, int vallen) {
    return -1;
}

const sock_setopt setopt_vfptr[] = {
    set_noblock,
    set_sndwin,
    set_rcvwin,
    0,
    0,
    set_linger,
    set_sndtimeo,
    set_rcvtimeo,
    set_reconnect,
    0,
    0,
};

int xsetopt(int fd, int level, int opt, void *val, int vallen) {
    int rc;
    struct sockbase *self = xget(fd);

    if (!self) {
	errno = EBADF;
	return -1;
    }
    if ((level != XL_SOCKET && !self->vfptr->setopt)
	|| (level == XL_SOCKET && (opt >= NELEM(setopt_vfptr, sock_setopt)
				   || !setopt_vfptr[opt]))) {
	xput(fd);
	errno = EINVAL;
	return -1;
    }
    switch (level) {
    case XL_SOCKET:
	rc = setopt_vfptr[opt](self, val, vallen);
	break;
    default:
	rc = self->vfptr->setopt(self, level, opt, val, vallen);
	break;
    }
    xput(fd);
    return rc;
}

