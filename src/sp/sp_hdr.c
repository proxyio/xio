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
#include <stdarg.h>
#include "sp_hdr.h"

struct sphdr *sphdr_new(u8 protocol, u8 version) {
    struct sphdr *sp_hdr = (struct sphdr *)xallocubuf(sizeof(*sp_hdr));
    if (sp_hdr) {
	sp_hdr->protocol = protocol;
	sp_hdr->version = version;
	sp_hdr->ttl = 0;
	sp_hdr->end_ttl = 0;
	sp_hdr->go = 0;
	sp_hdr->size = 0;
	sp_hdr->timeout = 0;
	sp_hdr->sendstamp = 0;
    }
    return sp_hdr;
}

void sphdr_free(struct sphdr *sp_hdr) {
    xfreeubuf((char *)sp_hdr);
}
