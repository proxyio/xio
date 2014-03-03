#ifndef _SLICE_H_
#define _SLICE_H_

struct slice {
    uint32_t len;
    char *data;
}; 


#define slice_bufcap(n, s, cap)			\
    {						\
	int __i;				\
	struct slice *__s = s;			\
	for (__i = 0; __i < n; __i++, __s++)	\
	    cap += __s->len;			\
    }						\



#define locate_slice_buffer(idx, s, buffer, cap)	\
    {							\
	struct slice *__s = s;				\
	uint32_t __idx = idx, __size = __s->len;	\
	while (__idx >= __size) {			\
	    __idx -= __size;				\
	    __s++;					\
	    __size = __s->len;				\
	}						\
 	buffer = __s->data + __idx;			\
	cap = __size - __idx;				\
    }


#endif  // _SLICE_H_
