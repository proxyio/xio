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

#ifndef _H_PROXYIO_EV_STATS_
#define _H_PROXYIO_EV_STATS_

#include <utils/mstats_base.h>

enum {
	ST_EV_IN = 0,
	ST_EV_OUT,
	ST_EV_ERR,
	ST_EV_NOTHG,
	ST_EV_KEYRANGE,
};

void ev_s_warn (struct mstats_base *stb, int sl, int key, i64 thres,
		    i64 val, i64 min_val, i64 max_val, i64 avg_val);

void ev_m_warn (struct mstats_base *stb, int sl, int key, i64 thres,
		    i64 val, i64 min_val, i64 max_val, i64 avg_val);

void ev_h_warn (struct mstats_base *stb, int sl, int key, i64 thres,
		    i64 val, i64 min_val, i64 max_val, i64 avg_val);

void ev_d_warn (struct mstats_base *stb, int sl, int key, i64 thres,
		    i64 val, i64 min_val, i64 max_val, i64 avg_val);

DEFINE_MSTATS (ev, ST_EV_KEYRANGE);


#endif
