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

/* Rex: a portable socket library */

#ifndef _H_PROXYIO_REX_IF_
#define _H_PROXYIO_REX_IF_

#include <utils/list.h>

/* The underlying rex socket vfptr */
struct rex_vfptr {
	int un_family;
	int (*init)    (struct rex_sock *rs);
	int (*destroy) (struct rex_sock *rs);
	int (*listen)  (struct rex_sock *rs, const char *sock);
	int (*accept)  (struct rex_sock *rs, struct rex_sock *new);
	int (*connect) (struct rex_sock *rs, const char *peer);
	int (*send)    (struct rex_sock *rs, struct rex_iov *iov, int niov);
	int (*recv)    (struct rex_sock *rs, struct rex_iov *iov, int niov);
	int (*setopt)  (struct rex_sock *rs, int opt, void *optval, int optlen);
	int (*getopt)  (struct rex_sock *rs, int opt, void *optval, int *optlen);
	struct list_head item;
};

struct rex_vfptr *get_rex_vfptr (int un_family);

#endif
