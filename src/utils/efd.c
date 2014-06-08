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

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include "base.h"
#include "efd.h"

int efd_init(struct efd *self)
{
	int rc;
	int flags;
	int p[2];

	if ((rc = pipe(p)) < 0)
		return -1;
	self->r = p[0];
	self->w = p[1];

	flags = fcntl(self->r, F_GETFL, 0);
	if (flags == -1)
		flags = 0;
	if ((rc = fcntl(self->r, F_SETFL, flags | O_NONBLOCK)) < 0) {
		efd_destroy(self);
		return -1;
	}

	flags = fcntl(self->w, F_GETFL, 0);
	if (flags == -1)
		flags = 0;
	if ((rc = fcntl(self->w, F_SETFL, flags | O_NONBLOCK)) < 0) {
		efd_destroy(self);
		return -1;
	}
	return 0;
}

void efd_destroy(struct efd *self)
{
	close(self->r);
	close(self->w);
}

int efd_signal(struct efd *self)
{
	int rc;
	char c = 94;

	rc = write(self->w, &c, 1);
	return rc;
}

void efd_unsignal(struct efd *self)
{
	ssize_t nbytes;
	u8 buf[128];

	while(1) {
		nbytes = read(self->r, buf, sizeof(buf));
		if (nbytes < 0 && errno == EAGAIN)
			nbytes = 0;
		BUG_ON(nbytes < 0);
		if ((size_t) nbytes < sizeof(buf))
			break;
	}
}

