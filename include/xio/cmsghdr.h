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

#ifndef _H_PROXYIO_CMSGHDR_
#define _H_PROXYIO_CMSGHDR_

#include <inttypes.h>
#include <xio/cplusplus_define.h>

char *ualloc (int size);
void ufree (char *ubuf);
int usize (char *ubuf);

/* One normal ubuf can take at most 16 control ubuf. */
enum {
	/* Following options are operate on control ubuf */	
	SNUM = 1,
	SFIRST,
	SNEXT,
	SLAST,
	SADD,
	SRM,
	SSWITCH,
	SCOPY,
};

int uctl (char *ubuf, int opt, void *optval);

static inline void uctl_add (char *ubuf, char *add)
{
	uctl (ubuf, SADD, add);
}

#include <xio/cplusplus_endif.h>
#endif
