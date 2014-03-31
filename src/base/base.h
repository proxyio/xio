#ifndef _HPIO_BASE_
#define _HPIO_BASE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


void base_init();
void base_exit();

#define true 1
#define false 0

#define BUG_ON(condition) do {					\
	if (condition) {					\
	    printf("%s:%d %s()", __FILE__, __LINE__, __func__);	\
	    abort();						\
	}							\
    } while (0)

#define PATH_MAX 4096
#define PAGE_SIZE 4096
#define ZERO(x) memset(&(x), 0, sizeof(x))
#define ERROR (-1 & __LINE__)
#define STREQ(a, b) (strlen(a) == strlen(b) && memcmp(a, b , strlen(a)) == 0)

// Get offset of a member
#define __offsetof(TYPE, MEMBER) ((long) &(((TYPE *)0)->MEMBER))

// Casts a member of a structure out to the containning structure
#define cont_of(ptr, type, member) ({					\
	    (type *)((char *)ptr - __offsetof(type, member)); })

typedef struct io {
    int64_t (*read)(struct io *c, char *buf, int64_t size);
    int64_t (*write)(struct io *c, char *buf, int64_t size);
} io_t;



#endif
