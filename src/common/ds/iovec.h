#ifndef _HPIO_SLICE_
#define _HPIO_SLICE_

#include <sys/uio.h>

#define iovecs_cap(n, s, cap)			\
    {						\
	int __i;				\
	struct iovec *__s = s;			\
	for (__i = 0; __i < n; __i++, __s++)	\
	    cap += __s->iov_len;		\
    }						\



#define locate_iovecs_buffer(idx, s, buffer, cap)	\
    {							\
	struct iovec *__s = s;				\
	uint32_t __idx = idx, __size = __s->iov_len;	\
	while (__idx >= __size) {			\
	    __idx -= __size;				\
	    __s++;					\
	    __size = __s->iov_len;			\
	}						\
 	buffer = (char *)__s->iov_base + __idx;		\
	cap = __size - __idx;				\
    }


#endif  // _SLICE_H_
