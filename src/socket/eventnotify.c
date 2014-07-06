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

int check_pollevents (struct sockbase *sb, int events)
{
	int happened = 0;

	if (events & XPOLLIN) {
		if (sb->vfptr->type == XCONNECTOR)
			happened |= !list_empty (&sb->rcv.head) ? XPOLLIN : 0;
		else if (sb->vfptr->type == XLISTENER)
			happened |= !list_empty (&sb->acceptq.head) ? XPOLLIN : 0;
	}
	if (events & XPOLLOUT)
		happened |= msgbuf_can_in (&sb->snd) ? XPOLLOUT : 0;
	if (events & XPOLLERR)
		happened |= sb->fepipe ? XPOLLERR : 0;
	DEBUG_OFF ("%d happen %s", sb->fd, xpoll_str[happened]);
	return happened;
}

/* Generic xpoll_t notify function. always called by sockbase_vfptr
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and proto_spec saved the other
 * events gived by sockbase_vfptr
 */
void __emit_pollevents (struct sockbase *sb)
{
	int happened = 0;
	struct pollbase *pb, *tmp;

	happened |= check_pollevents (sb, XPOLLIN|XPOLLOUT|XPOLLERR);
	walk_pollbase_s (pb, tmp, &sb->poll_entries) {
		BUG_ON (!pb->vfptr);
		pb->vfptr->emit (pb, happened & pb->pollfd.events);
	}
}

void emit_pollevents (struct sockbase *sb)
{
	mutex_lock (&sb->lock);
	__emit_pollevents (sb);
	mutex_unlock (&sb->lock);
}
