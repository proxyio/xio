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

#ifndef _H_PROXYIO_EVENTPOLL_
#define _H_PROXYIO_EVENTPOLL_

#include <config.h>

struct eventpoll;

/* store the event data for file descriptor, the underlying impl must contain
   three fields, fdd->fd|fdd->events|fdd->ready_events */
struct fdd;

void fdd_init (struct fdd *fdd);
void fdd_term (struct fdd *fdd);

/* initialize the underlying eventpoll */
void eventpoll_init (struct eventpoll *evp);

/* terminate the underlying eventpoll, all the registered fd will be removed */
void eventpoll_term (struct eventpoll *evp);

int eventpoll_ctl (struct eventpoll *evp, int op /* EVP_ADD|EVP_DEL|EVP_MOD */,
		   struct fdd *fdd);

/* waiting events happened for a maximum time of timeout milliseconds */
int eventpoll_wait (struct eventpoll *evp, struct fdd **fdds, int max, int timeout);



#if defined USE_EPOLL

#include "ev_epoll.h"

#elif defined USE_SELECT

#include "ev_select.h"

#elif defined USE_POLL

#include "ev_poll.h"

#elif defined USE_KQUEUE

#include "ev_kqueue.h"

#elif defined HAVE_EPOLL_CREATE

#include "ev_epoll.h"

#elif defined HAVE_SELECT

#include "ev_select.h"

#elif defined HAVE_POLL

#include "ev_poll.h"

#elif defined HAVE_KQUEUE

#include "ev_kqueue.h"

#endif

#endif
