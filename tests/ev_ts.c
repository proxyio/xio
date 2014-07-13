#include <stdio.h>
#include <ev/ev.h>

static int size = 0;

static void stdin_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	BUG_ON (events != 0);
	size++;
}

static void stdout_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	BUG_ON (events != EV_WRITE);
	size++;
}

static void stderr_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	BUG_ON (events != EV_WRITE);
	size++;
}

int main (int argc, char **argv)
{
	struct ev_fd ev_fd[3];
	struct ev_fdset fds;

	ev_fdset_init (&fds);
	ev_fd_init (&ev_fd[0]);
	ev_fd_init (&ev_fd[1]);
	ev_fd_init (&ev_fd[2]);

	ev_fd[0].fd = 0;
	ev_fd[0].events = EV_READ;
	ev_fd[0].hndl = stdin_hndl;

	ev_fd[1].fd = 1;
	ev_fd[1].events = EV_READ|EV_WRITE;
	ev_fd[1].hndl = stdout_hndl;

	ev_fd[2].fd = 2;
	ev_fd[2].events = EV_READ|EV_WRITE;
	ev_fd[2].hndl = stderr_hndl;

	__ev_fdset_ctl (&fds, EV_ADD, &ev_fd[0]);
	__ev_fdset_ctl (&fds, EV_ADD, &ev_fd[1]);
	__ev_fdset_ctl (&fds, EV_ADD, &ev_fd[2]);

	ev_fdset_poll (&fds, 10);
	__ev_fdset_ctl (&fds, EV_DEL, &ev_fd[0]);
	ev_fdset_poll (&fds, 10);

	ev_fdset_term (&fds);
	return 0;
}
