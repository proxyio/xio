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

#ifndef _H_PROXYIO_SP_REQREP_
#define _H_PROXYIO_SP_REQREP_

#include <xio/sp.h>
#include <xio/cplusplus_define.h>

#define SP_REQREP_VERSION 0x0001

/* Following sp_types are provided by REQREP protocol */
#define SP_REQ         1
#define SP_REP         2

/* Following options are provided by SP_REQ */
enum {
	SP_PROXY  =   0,      /* building proxy between SP_REP and SP_REQ */
	SP_REQ_LBS,           /* specified the loadbalance strategy for SP_REQ */
	SP_REQ_RRBIN_WEIGHT,
};

/* Following options are provided by SP_REP */


/* Following loadbalance strategies are provided by SP_REQ */
enum {
	SP_REQ_RRBIN  =  0,
	SP_REQ_ULHASH,
	SP_REQ_CONSISTENT_HASH,
};

struct rrbin_attr {
	int fd;
	int weight;
};

typedef uint32_t (*ulhash) (char *ubuf);











#include <xio/cplusplus_endif.h>
#endif
