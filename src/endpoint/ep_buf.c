#include <stdio.h>
#include <stdarg.h>
#include "ep_struct.h"

void *xep_allocbuf(int flags, int size, ...) {
    va_list ap;
    struct ephdr *h1 = 0, *h2 = 0;

    if (flags & XEPBUF_CLONEHDR) {
	va_start(ap, size);
	h1 = udata2ephdr(va_arg(ap, void *));
	va_end(ap);
    }
    size += h1 ? ephdr_ctlen(h1) : 0;
    if (!(h2 = (struct ephdr *)xallocmsg(size)))
	return 0;
    memcpy((void *)h2, (void *)h1, ephdr_ctlen(h1));
    return ephdr2udata(h2);
}

void xep_freebuf(void *buf) {
}
