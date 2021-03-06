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

#include "sio.h"

static int get_noblock(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->flagset.non_block ? true : false;
    mutex_unlock(&sb->lock);
    return 0;
}

static int get_nodelay(struct sockbase* sb, void* optval, int* optlen) {
    int rc;
    struct sio* tcps = cont_of(sb, struct sio, base);

    mutex_lock(&sb->lock);
    rc = rex_sock_getopt(&tcps->s, REX_SO_NODELAY, optval, optlen);
    mutex_unlock(&sb->lock);
    return rc;
}

static int get_sndbuf(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->snd.wnd;
    mutex_unlock(&sb->lock);
    return 0;
}

static int get_rcvbuf(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->rcv.wnd;
    mutex_unlock(&sb->lock);
    return 0;
}

static int get_linger(struct sockbase* sb, void* optval, int* optlen) {
    int rc;
    struct sio* tcps = cont_of(sb, struct sio, base);

    mutex_lock(&sb->lock);
    rc = rex_sock_getopt(&tcps->s, REX_SO_LINGER, optval, optlen);
    mutex_unlock(&sb->lock);
    return rc;
}

static int get_sndtimeo(struct sockbase* sb, void* optval, int* optlen) {
    return -1;
}

static int get_rcvtimeo(struct sockbase* sb, void* optval, int* optlen) {
    return -1;
}

static int get_socktype(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->vfptr->type;
    mutex_unlock(&sb->lock);
    return 0;
}

static int get_sockpf(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->vfptr->pf;
    mutex_unlock(&sb->lock);
    return 0;
}

static int get_verbose(struct sockbase* sb, void* optval, int* optlen) {
    mutex_lock(&sb->lock);
    * (int*) optval = sb->flagset.verbose;
    mutex_unlock(&sb->lock);
    return 0;
}


static const sock_geter getopt_vfptr[] = {
    get_linger,
    get_sndbuf,
    get_rcvbuf,
    get_noblock,
    get_nodelay,
    get_sndtimeo,
    get_rcvtimeo,
    get_socktype,
    get_sockpf,
    get_verbose,
};

int sio_getopt(struct sockbase* sb, int opt, void* optval, int* optlen) {
    int rc;

    if (opt >= NELEM(getopt_vfptr) || !getopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }

    rc = getopt_vfptr[opt](sb, optval, optlen);
    return rc;
}



static int set_noblock(struct sockbase* sb, void* optval, int optlen) {
    mutex_lock(&sb->lock);
    sb->flagset.non_block = * (int*) optval ? true : false;
    mutex_unlock(&sb->lock);
    return 0;
}

static int set_nodelay(struct sockbase* sb, void* optval, int optlen) {
    int rc;
    struct sio* tcps = cont_of(sb, struct sio, base);

    mutex_lock(&sb->lock);
    rc = rex_sock_setopt(&tcps->s, REX_SO_NODELAY, optval, optlen);
    mutex_unlock(&sb->lock);
    return rc;
}

static int set_sndbuf(struct sockbase* sb, void* optval, int optlen) {
    mutex_lock(&sb->lock);
    sb->snd.wnd = (* (int*) optval);
    mutex_unlock(&sb->lock);
    return 0;
}

static int set_rcvbuf(struct sockbase* sb, void* optval, int optlen) {
    mutex_lock(&sb->lock);
    sb->rcv.wnd = (* (int*) optval);
    mutex_unlock(&sb->lock);
    return 0;
}

static int set_linger(struct sockbase* sb, void* optval, int optlen) {
    int rc;
    struct sio* tcps = cont_of(sb, struct sio, base);

    mutex_lock(&sb->lock);
    rc = rex_sock_setopt(&tcps->s, REX_SO_LINGER, optval, optlen);
    mutex_unlock(&sb->lock);
    return rc;
}

static int set_sndtimeo(struct sockbase* sb, void* optval, int optlen) {
    return -1;
}

static int set_rcvtimeo(struct sockbase* sb, void* optval, int optlen) {
    return -1;
}

static int set_verbose(struct sockbase* sb, void* optval, int optlen) {
    mutex_lock(&sb->lock);
    sb->flagset.verbose =  * (int*) optval & 0xf;
    mutex_unlock(&sb->lock);
    return 0;
}

static const sock_seter setopt_vfptr[] = {
    set_linger,
    set_sndbuf,
    set_rcvbuf,
    set_noblock,
    set_nodelay,
    set_sndtimeo,
    set_rcvtimeo,
    0,
    0,
    set_verbose,
};

int sio_setopt(struct sockbase* sb, int opt, void* optval, int optlen) {
    int rc;

    if (opt >= NELEM(setopt_vfptr) || !setopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }

    rc = setopt_vfptr[opt](sb, optval, optlen);
    return rc;
}
