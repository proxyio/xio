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


#include "ep_struct.h"

struct endsock *endsock_new() {
    struct endsock *sk = (struct endsock *)mem_zalloc(sizeof(*sk));
    return sk;
}

void endsock_free(struct endsock *sk) {
    mem_free(sk, sizeof(*sk));
}

struct endpoint *endpoint_new() {
    struct endpoint *ep = (struct endpoint *)mem_zalloc(sizeof(*ep));

    if (ep) {
	ep->owner = 0;
	ep->type = 0;
	INIT_LIST_HEAD(&ep->listeners);
	INIT_LIST_HEAD(&ep->connectors);
	INIT_LIST_HEAD(&ep->bad_socks);
    }
    return ep;
}


void endpoint_free(struct endpoint *ep) {
    mem_free(ep, sizeof(*ep));
}


struct proxy *proxy_new() {
    struct proxy *py = (struct proxy *)mem_zalloc(sizeof(*py));

    if (py) {
	if (!(py->po = xpoll_create())) {
	    mem_free(py, sizeof(*py));
	    return 0;
	}
	py->exiting = true;
	py->ref = 0;
	spin_init(&py->lock);
	py->frontend = 0;
	py->backend = 0;
    }
    return py;
}

void proxy_free(struct proxy *py) {
    spin_destroy(&py->lock);
    xpoll_close(py->po);
    mem_free(py, sizeof(*py));
}


int proxy_get(struct proxy *py) {
    int ref;
    spin_lock(&py->lock);
    ref = py->ref++;
    spin_unlock(&py->lock);
    return ref;
}


int proxy_put(struct proxy *py) {
    int ref;
    spin_lock(&py->lock);
    ref = py->ref--;
    spin_unlock(&py->lock);
    return ref;
}
