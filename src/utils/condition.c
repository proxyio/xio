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

#include "timer.h"
#include "condition.h"

int condition_init(condition_t *cond)
{
    return pthread_cond_init(&cond->_cond, NULL);
}

int condition_destroy(condition_t *cond)
{
    return pthread_cond_destroy(&cond->_cond);
}

int condition_timedwait(condition_t *cond, mutex_t *mutex, int to)
{
    struct timespec ts;
    u64 endlife = gettimeof(ns) + (u64)to * 1000000;

    /* Abstime for pthread timedwait */
    ts.tv_sec = endlife / 1000000000;
    ts.tv_nsec = endlife % 1000000000;
    return pthread_cond_timedwait(&cond->_cond, &mutex->_mutex, &ts);
}

int condition_wait(condition_t *cond, mutex_t *mutex)
{
    return pthread_cond_wait(&cond->_cond, &mutex->_mutex);
}


int condition_signal(condition_t *cond)
{
    return pthread_cond_signal(&cond->_cond);
}

int condition_broadcast(condition_t *cond)
{
    return pthread_cond_broadcast(&cond->_cond);
}
