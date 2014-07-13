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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "global.h"

/* Backend poller wait kernel timeout msec */
#define DEF_ELOOPTIMEOUT 1
#define DEF_ELOOPIOMAX 100

struct xglobal xgb = {};

static void __shutdown_socks_task_hndl (struct worker *cpu)
{
	struct task_ent *te, *tmp;
	struct list_head st_head = {};

	INIT_LIST_HEAD (&st_head);
	worker_lock (cpu);
	efd_unsignal (&cpu->efd);
	list_splice (&cpu->shutdown_socks, &st_head);
	worker_unlock (cpu);

	walk_task_s (te, tmp, &st_head) {
		list_del_init (&te->link);
		te->f (te);
	}
}

static int cpu_task_hndl (eloop_t *el, ev_t *et)
{
	return 0;
}

volatile static int kcpud_exits = 0;

static inline int kcpud (void *args)
{
	waitgroup_t *wg = (waitgroup_t *) args;
	int rc = 0;
	int cpu_no = worker_alloc();
	struct worker *cpu = get_worker (cpu_no);

	worker_lock_init (cpu);
	INIT_LIST_HEAD (&cpu->shutdown_socks);

	/* Init eventloop and wakeup parent */
	BUG_ON (eloop_init (&cpu->el, PROXYIO_MAX_SOCKS/PROXYIO_MAX_CPUS,
	                    DEF_ELOOPIOMAX, DEF_ELOOPTIMEOUT) != 0);
	BUG_ON (efd_init (&cpu->efd) );
	ZERO (cpu->efd_et);
	cpu->efd_et.events = EPOLLIN|EPOLLERR;
	cpu->efd_et.fd = cpu->efd.r;
	cpu->efd_et.f = cpu_task_hndl;
	cpu->efd_et.data = cpu;
	BUG_ON (eloop_add (&cpu->el, &cpu->efd_et) != 0);

	/* Init done. wakeup parent thread */
	waitgroup_done (wg);

	while (!xgb.exiting) {
		eloop_once (&cpu->el);
		__shutdown_socks_task_hndl (cpu);
		BUG_ON (xgb.exiting != 0 && xgb.exiting != 1);
	}
	while (!list_empty (&cpu->shutdown_socks) )
		__shutdown_socks_task_hndl (cpu);
	kcpud_exits++;

	BUG_ON (!list_empty (&cpu->shutdown_socks) );
	/* Release the poll descriptor when kcpud exit. */
	worker_free (cpu_no);
	eloop_destroy (&cpu->el);
	worker_lock_destroy (cpu);
	return rc;
}


struct sockbase_vfptr *sockbase_vfptr_lookup (int pf, int type) {
	struct sockbase_vfptr *vfptr, *ss;

	walk_sockbase_vfptr_s (vfptr, ss, &xgb.sockbase_vfptr_head) {
		if (pf == vfptr->pf && vfptr->type == type)
			return vfptr;
	}
	return 0;
}

extern struct sockbase_vfptr mix_listener_vfptr;

void socket_module_init()
{
	waitgroup_t wg;
	int fd;
	int cpu_no;
	int i;
	struct list_head *protocol_head = &xgb.sockbase_vfptr_head;

	xgb.exiting = false;
	mutex_init (&xgb.lock);

	for (fd = 0; fd < PROXYIO_MAX_SOCKS; fd++)
		xgb.unused[fd] = fd;
	for (cpu_no = 0; cpu_no < PROXYIO_MAX_CPUS; cpu_no++)
		xgb.cpu_unused[cpu_no] = cpu_no;

	xgb.cpu_cores = 1;
	taskpool_init (&xgb.tpool, xgb.cpu_cores);
	taskpool_start (&xgb.tpool);

	waitgroup_init (&wg);
	waitgroup_adds (&wg, xgb.cpu_cores);
	for (i = 0; i < xgb.cpu_cores; i++)
		taskpool_run (&xgb.tpool, kcpud, &wg);
	/* Waiting all poll's kcpud start properly */
	waitgroup_wait (&wg);
	waitgroup_term (&wg);

	/* The priority of sockbase_vfptr: inproc > ipc > tcp */
	INIT_LIST_HEAD (protocol_head);
	list_add_tail (&inproc_listener_vfptr.link, protocol_head);
	list_add_tail (&inproc_connector_vfptr.link, protocol_head);
	list_add_tail (&ipc_listener_vfptr.link, protocol_head);
	list_add_tail (&ipc_connector_vfptr.link, protocol_head);
	list_add_tail (&tcp_listener_vfptr.link, protocol_head);
	list_add_tail (&tcp_connector_vfptr.link, protocol_head);
	list_add_tail (&mix_listener_vfptr.link, protocol_head);
}

void socket_module_exit()
{
	DEBUG_OFF();
	xgb.exiting = true;
	taskpool_stop (&xgb.tpool);
	BUG_ON (xgb.nsockbases);
	taskpool_destroy (&xgb.tpool);
	mutex_destroy (&xgb.lock);
}
