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

#ifndef _H_PROXYIO_BASE_
#define _H_PROXYIO_BASE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <inttypes.h>
#include <sys/syscall.h>

#define foreach(i, max) for (i = 0; i < max; i++)

#if !defined gettid
int gettid();
#endif

#define true 1
#define false 0

#define BUG_ON(condition) do {					\
	if (condition) {					\
	    printf("%s:%d %s() %s false with errno: %d", __FILE__,	\
		   __LINE__, __func__, #condition, errno);	\
	    abort();						\
	}							\
    } while (0)

#define DEBUG_ON(fmt, ...) do {						\
	fprintf(stdout, "%d %s:%d %s "#fmt"\n", (i32)gettid(),		\
		basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__);	\
    } while(0)

#define DEBUG_OFF(fmt, ...)

#define PATH_MAX 4096
#define PAGE_SIZE 4096
#define ZERO(x) memset(&(x), 0, sizeof(x))
#define ERROR (-1 & __LINE__)
#define STREQ(a, b) (strlen(a) == strlen(b) && memcmp(a, b , strlen(a)) == 0)

// Get offset of a member
#if !defined(__offsetof)
#define __offsetof(TYPE, MEMBER) ((long) &(((TYPE *)0)->MEMBER))
#endif

// Casts a member of a structure out to the containning structure
#define cont_of(ptr, type, member) ({					\
	    (type *)((char *)ptr - __offsetof(type, member)); })

typedef struct io {
	int64_t (*read) (struct io *c, char *buf, int64_t size);
	int64_t (*write) (struct io *c, char *buf, int64_t size);
} io_t;


typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define NELEM(x, type) (sizeof(x) / sizeof(type))

#define MAX(a, b) (a > b ? a : b)

#define MIN(a, b) (a > b ? b : a)

#define ERRNO_RETURN(eno) do {			\
		errno = eno;			\
		return -1;			\
	} while (0)



#endif
