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

int xclose(int fd) {
    struct sockbase *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);
    
    if (!self) {
	errno = EBADF;
	return -1;
    }
    mutex_lock(&cpu->lock);
    mutex_lock(&self->lock);

    while (efd_signal(&cpu->efd) < 0)
	mutex_relock(&cpu->lock);
    self->fepipe = true;
    list_add_tail(&self->shutdown.link, &cpu->shutdown_socks);

    if (self->rcv.waiters || self->snd.waiters)
	condition_broadcast(&self->cond);

    mutex_unlock(&self->lock);
    mutex_unlock(&cpu->lock);
    xput(fd);
    return 0;
}
