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


int xsetsockopt(int xd, int level, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	sx->fasync = *(int *)on ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	sx->snd_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	sx->rcv_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

