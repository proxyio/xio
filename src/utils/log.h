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

#ifndef _H_PROXYIO_LOG_
#define _H_PROXYIO_LOG_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

enum {
	DEBUGL   =   1,
	INFOL,
	NOTICEL,
	WARNL,
	ERRORL,
	FATALL,
};


#define LLOG(lvs, lv, fmt, ...) do {					\
        if (lvs && lvs > lv) {							\
            fprintf(stdout, "%d %s:%d %s "#fmt"\n", (i32)gettid(),	\
            basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__);	\
        }								\
    } while(0)

#define LOG_DEBUG(lvs, fmt, ...)   LLOG(lvs, DEBUGL,  fmt, ##__VA_ARGS__)

#define LOG_INFO(lvs, fmt, ...)    LLOG(lvs, INFOL,   fmt, ##__VA_ARGS__)

#define LOG_NOTICE(lvs, fmt, ...)  LLOG(lvs, NOTICEL, fmt, ##__VA_ARGS__)

#define LOG_WARN(lvs, fmt, ...)    LLOG(lvs, WARNL,   fmt, ##__VA_ARGS__)

#define LOG_ERROR(lvs, fmt, ...)   LLOG(lvs, ERRORL,  fmt, ##__VA_ARGS__)

#define LOG_FATAL(lvs, fmt, ...)   LLOG(lvs, FATALL,  fmt, ##__VA_ARGS__)

#endif
