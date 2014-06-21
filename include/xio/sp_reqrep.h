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

/* Following options are provided by REQREP protocol */
#define SP_PROXY            1  /* recv message from rep_ep and dispatch to req_ep */
#define SP_REQ_TGALGO       2  /* specified the loadbalance algo for SP_REQ */
#define SP_REQ_WRE          3  /* struct sp_req_rrbin_weight_entry */

/* Following loadbalance algos are provided by REQREP protocol */
#define SP_REQ_RRBIN          1
#define SP_REQ_WEIGHT_RRBIN   2
#define SP_REQ_RTSLOAD        3   /* real-time loadbalance by second statistics */
#define SP_REQ_RTHLOAD        4   /* real-time loadbalance by minute statistics */
#define SP_REQ_RTMLOAD        5   /* real-time loadbalance by hour statistics */
#define SP_REQ_RTDLOAD        6   /* real-time loadbalance by day statistics */

struct sp_req_rrbin_weight_entry {
	int fd;
	int weight;
};

#include <xio/cplusplus_endif.h>
#endif
