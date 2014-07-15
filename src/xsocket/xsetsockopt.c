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

typedef int (*sock_setopt) (struct sockbase *sb, void *optval, int optlen);

static int set_noblock (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->flagset.non_block = * (int *) optval ? true : false;
	mutex_unlock (&sb->lock);
	return 0;
}

static int set_sndwin (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->snd.wnd = (* (int *) optval);
	mutex_unlock (&sb->lock);
	return 0;
}

static int set_rcvwin (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->rcv.wnd = (* (int *) optval);
	mutex_unlock (&sb->lock);
	return 0;
}

static int set_linger (struct sockbase *sb, void *optval, int optlen)
{
	return -1;
}

static int set_sndtimeo (struct sockbase *sb, void *optval, int optlen)
{
	return -1;
}

static int set_rcvtimeo (struct sockbase *sb, void *optval, int optlen)
{
	return -1;
}

static int set_reconnect (struct sockbase *sb, void *optval, int optlen)
{
	return -1;
}

static int set_debuglv (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->flagset.debuglv =  * (int *) optval ? true : false;
	mutex_unlock (&sb->lock);
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
	set_debuglv,
};

static int _setopt (struct sockbase *sb, int opt, void *optval, int optlen)
{
	int rc;
	if (opt >= NELEM (setopt_vfptr, sock_setopt) || !setopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = setopt_vfptr[opt] (sb, optval, optlen);
	return rc;
}

static int _tp_setopt (struct sockbase *sb, int level, int opt, void *optval,
                       int optlen)
{
	int rc;
	if (!sb->vfptr->setopt) {
		errno = EINVAL;
		return -1;
	}
	rc = sb->vfptr->setopt (sb, level, opt, optval, optlen);
	return rc;
}

int xsetopt (int fd, int level, int opt, void *optval, int optlen)
{
	int rc;
	struct sockbase *sb = xget (fd);

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	switch (level) {
	case XL_SOCKET:
		rc = _setopt (sb, opt, optval, optlen);
		break;
	default:
		rc = _tp_setopt (sb, level, opt, optval, optlen);
		break;
	}
	xput (fd);
	return rc;
}

