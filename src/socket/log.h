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

#ifndef _H_PROXYIO_SOCKET_LOG_
#define _H_PROXYIO_SOCKET_LOG_

#include <utils/log.h>


#define SKLOG_DEBUG(s, fmt, ...)   LOG_DEBUG((s)->flagset.debuglv, fmt, ##__VA_ARGS__)

#define SKLOG_INFO(s, fmt, ...)    LOG_INFO((s)->flagset.debuglv, fmt, ##__VA_ARGS__)

#define SKLOG_NOTICE(s, fmt, ...)  LOG_NOTICE((s)->flagset.debuglv, fmt, ##__VA_ARGS__)

#define SKLOG_WARN(s, fmt, ...)    LOG_WARN((s)->flagset.debuglv, fmt, ##__VA_ARGS__)

#define SKLOG_ERROR(s, fmt, ...)   LOG_ERROR((s)->flagset.debuglv, fmt, ##__VA_ARGS__)

#define SKLOG_FATAL(s, fmt, ...)   LOG_FATAL((s)->flagset.debuglv, fmt, ##__VA_ARGS__)


#endif
