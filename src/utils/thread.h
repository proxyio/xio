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

#ifndef _XIO_THREAD_
#define _XIO_THREAD_

#include <pthread.h>
#include "base.h"
#include "alloc.h"

/* TODO: remove platform specified code from here */
#include <unistd.h>
#include <sys/syscall.h>




typedef int (*thread_func)(void *);
typedef struct thread {
    pid_t krid;
    pthread_t tid;
    thread_func f;
    void *args;
    int res;
} thread_t;

static inline thread_t *thread_new()
{
    thread_t *tt = (thread_t *)mem_zalloc(sizeof(*tt));
    return tt;
}

int thread_start(thread_t *tt, thread_func f, void *args);
int thread_stop(thread_t *tt);



#endif
