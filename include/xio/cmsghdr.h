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

#ifndef _XIO_XCMSGHDR_
#define _XIO_XCMSGHDR_

#include <inttypes.h>
#include <xio/cplusplus_define.h>

enum {
	SNUM = 1,
	SFIRST,
	SNEXT,
	SLAST,
	SADD,
	SRM,
	SCLONE,
};

int ubufctl (char *ubuf, int opt, void *optval);

static inline int ubufctl_num (char *ubuf)
{
	int sub_num = 0;
	ubufctl (ubuf, SNUM, &sub_num);
	return sub_num;
}

static inline char *ubufctl_first (char *ubuf)
{
	char *first = 0;
	ubufctl (ubuf, SFIRST, &first);
	return first;
}

static inline char *ubufctl_next (char *ubuf)
{
	ubufctl (ubuf, SNEXT, &ubuf);
	return ubuf;
}

static inline char *ubufctl_last (char *ubuf)
{
	char *last = 0;
	ubufctl (ubuf, SLAST, &last);
	return last;
}

static inline void ubufctl_add (char *ubuf, char *add)
{
	ubufctl (ubuf, SADD, add);
}

static inline void ubufctl_rm (char *ubuf, char *rm)
{
	ubufctl (ubuf, SRM, rm);
}

#include <xio/cplusplus_endif.h>
#endif
