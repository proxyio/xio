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
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "xg.h"


typedef int (*sock_getopt) (struct sockbase *self, void *optval, int *optlen);

static int get_noblock (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->flagset.non_block ? true : false;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_sndwin (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->snd.wnd;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_rcvwin (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->rcv.wnd;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_sndbuf (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->snd.size;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_rcvbuf (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->rcv.size;
	mutex_unlock (&self->lock);
	return 0;
}


static int get_linger (struct sockbase *self, void *optval, int *optlen)
{
	return -1;
}

static int get_sndtimeo (struct sockbase *self, void *optval, int *optlen)
{
	return -1;
}

static int get_rcvtimeo (struct sockbase *self, void *optval, int *optlen)
{
	return -1;
}

static int get_reconnect (struct sockbase *self, void *optval, int *optlen)
{
	return -1;
}

static int get_socktype (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->vfptr->type;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_sockpf (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->vfptr->pf;
	mutex_unlock (&self->lock);
	return 0;
}

static int get_debuglv (struct sockbase *self, void *optval, int *optlen)
{
	mutex_lock (&self->lock);
	* (int *) optval = self->flagset.debuglv;
	mutex_unlock (&self->lock);
	return 0;
}


const sock_getopt getopt_vfptr[] = {
	get_noblock,
	get_sndwin,
	get_rcvwin,
	get_sndbuf,
	get_rcvbuf,
	get_linger,
	get_sndtimeo,
	get_rcvtimeo,
	get_reconnect,
	get_socktype,
	get_sockpf,
	get_debuglv,
};



static int _getopt (struct sockbase *sb, int opt, void *optval, int *optlen)
{
	int rc;
	if (opt >= NELEM (getopt_vfptr, sock_getopt) || !getopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = getopt_vfptr[opt] (sb, optval, optlen);
	return rc;
}

static int _tp_getopt (struct sockbase *sb, int level, int opt, void *optval,
                       int *optlen)
{
	int rc;
	if (!sb->vfptr->getopt) {
		errno = EINVAL;
		return -1;
	}
	rc = sb->vfptr->getopt (sb, level, opt, optval, optlen);
	return rc;
}

int xgetopt (int fd, int level, int opt, void *optval, int *optlen)
{
	int rc;
	struct sockbase *sb = xget (fd);

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	switch (level) {
	case XL_SOCKET:
		rc = _getopt (sb, opt, optval, optlen);
		break;
	default:
		rc = _tp_getopt (sb, level, opt, optval, optlen);
		break;
	}
	xput (fd);
	return rc;
}
