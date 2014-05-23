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

#include <utils/modstat.h>
#include <xio/socket.h>
#include <xio/sp_reqrep.h>

enum {
    SEND = 0,
    RECV,
    BC_KEYRANGE,
};

const char *key_items[] = {
    "SEND",
    "RECV",
};

DEFINE_MODSTAT(bc, BC_KEYRANGE);

static char role = 0;
static char *host = 0;
static int64_t timeout = 10000;
static int thread_num = 1;
static int connection_num = 1;

static char buff[102400] = {};
static int buf_len = 1024;

static int randstr(char *buf, int len) {
    int i, idx;
    char token[] = "qwertyuioplkjhgfdsazxcvbnm1234567890";

    for (i = 0; i < len; i++) {
	idx = rand() % strlen(token);
	buf[i] = token[idx];
    }
    return 0;
}

static int get_option(int argc, char* argv[]) {
    int rc;
    while ( (rc = getopt(argc, argv, "r:s:w:c:h:z")) != -1 ) {
        switch(rc) {
	case 'r':
	    role = optarg[0];
	    break;
	case 's':
	    buf_len = atoi(optarg);
	    break;
        case 'w':
	    if ((thread_num = atoi(optarg)) <= 0)
		thread_num = 1;
	    break;
	case 'c':
	    if ((connection_num = atoi(optarg)) <= 0)
		connection_num = 1;
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'z':
        default:
	    return -1;
        }
    }
    timeout += gettimeofms();
    return 0;
}

static void s_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val) {
    DEBUG_ON("%s %ld", key_items[key], val);
}
static void m_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val) {
    DEBUG_ON("%s %ld", key_items[key], val);
}
static void h_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val) {
    DEBUG_ON("%s %ld", key_items[key], val);
}
static void d_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val) {
    DEBUG_ON("%s %ld", key_items[key], val);
}

static void req_worker1(int eid) {
    int i, fd;
    int64_t now;
    char *ubuf;
    bc_modstat_t bcst = {};
    modstat_t *st = &bcst.self;

    INIT_MODSTAT(bcst);
    modstat_set_warnf(st, MSL_S, s_warn);
    modstat_set_warnf(st, MSL_M, m_warn);
    modstat_set_warnf(st, MSL_H, h_warn);
    modstat_set_warnf(st, MSL_D, d_warn);

    for (i = 0; i < connection_num; i++) {
	BUG_ON((fd = xconnect(host)) < 0);
	BUG_ON(sp_add(eid, fd));
    }
    while ((now = gettimeofms()) < timeout) {
	ubuf = xallocubuf(rand() % buf_len);
	memcpy(ubuf, buff, xubuflen(ubuf));
	BUG_ON(sp_send(eid, ubuf));
	modstat_incrkey(st, SEND);
	BUG_ON(sp_recv(eid, &ubuf));
	modstat_incrkey(st, RECV);
	xfreeubuf(ubuf);
	modstat_update_timestamp(st, now);
    }
}

static void req_worker2(int eid) {
    int fd;
    int64_t now;
    char *ubuf;
    bc_modstat_t bcst = {};
    modstat_t *st = &bcst.self;

    INIT_MODSTAT(bcst);
    modstat_set_warnf(st, MSL_S, s_warn);
    modstat_set_warnf(st, MSL_M, m_warn);
    modstat_set_warnf(st, MSL_H, h_warn);
    modstat_set_warnf(st, MSL_D, d_warn);

    BUG_ON((fd = xconnect(host)) < 0);
    DEBUG_ON("rep start send");
    while ((now = gettimeofms()) < timeout) {
	ubuf = xallocubuf(rand() % buf_len);
	memcpy(ubuf, buff, xubuflen(ubuf));
	BUG_ON(xsend(fd, ubuf));
	modstat_incrkey(st, SEND);
	BUG_ON(xrecv(fd, &ubuf));
	modstat_incrkey(st, RECV);
	xfreeubuf(ubuf);
	modstat_update_timestamp(st, now);
    }
    xclose(fd);
}

static void rep_worker1(int eid) {
    int fd;
    int64_t now;
    char *ubuf;
    bc_modstat_t bcst = {};
    modstat_t *st = &bcst.self;

    INIT_MODSTAT(bcst);
    modstat_set_warnf(st, MSL_S, s_warn);
    modstat_set_warnf(st, MSL_M, m_warn);
    modstat_set_warnf(st, MSL_H, h_warn);
    modstat_set_warnf(st, MSL_D, d_warn);

    BUG_ON((fd = xlisten(host)) < 0);
    BUG_ON(sp_add(eid, fd));
    while ((now = gettimeofms()) < timeout) {
	BUG_ON(sp_recv(eid, &ubuf));
	BUG_ON(sp_send(eid, ubuf));
	modstat_incrkey(st, RECV);
	modstat_update_timestamp(st, now);
    }
}

static void rep_worker2(int eid) {
    int fd, sfd;
    int64_t now;
    char *ubuf;
    bc_modstat_t bcst = {};
    modstat_t *st = &bcst.self;

    INIT_MODSTAT(bcst);
    modstat_set_warnf(st, MSL_S, s_warn);
    modstat_set_warnf(st, MSL_M, m_warn);
    modstat_set_warnf(st, MSL_H, h_warn);
    modstat_set_warnf(st, MSL_D, d_warn);

    BUG_ON((fd = xlisten(host)) < 0);
    BUG_ON((sfd = xaccept(fd)) < 0);
    DEBUG_ON("rep start recv");
    while ((now = gettimeofms()) < timeout) {
	BUG_ON(xrecv(sfd, &ubuf));
	BUG_ON(xsend(sfd, ubuf));
	modstat_incrkey(st, RECV);
	modstat_update_timestamp(st, now);
    }
    xclose(sfd);    
    xclose(fd);
}


int main(int argc, char  **argv) {
    int eid;

    randstr(buff, sizeof(buff));
    get_option(argc, argv);
    DEBUG_ON("start xio benchmark with {role:%c thread:%d connection:%d host:%s}",
	     role, thread_num, connection_num, host);
    BUG_ON(!role || !host);
    switch (role) {
    case 'c':
	eid = sp_endpoint(SP_REQREP, SP_REQ);
	BUG_ON(eid < 0);
	req_worker1(eid);
	sp_close(eid);
	break;
    case 's':
	eid = sp_endpoint(SP_REQREP, SP_REP);
	BUG_ON(eid < 0);
	rep_worker1(eid);
	sp_close(eid);
	break;
    default:
	BUG_ON(1);
    }
    return 0;
}
