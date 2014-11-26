#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ev/ev.h>

static int ev_in_size = 0;
static int ev_out_size = 0;

static void pipein_hndl(struct ev_fdset* evfds, struct ev_fd* evfd, int events) {
    BUG_ON(events != EV_READ);
    ev_in_size++;
}

static void pipeout_hndl(struct ev_fdset* evfds, struct ev_fd* evfd, int events) {
    BUG_ON(events != EV_WRITE);
    ev_out_size++;
}

int ev_test() {
    int pipefd[2];
    struct ev_fd ev_fd[2];
    struct ev_fdset fds;

    BUG_ON(pipe(pipefd));
    ev_fdset_init(&fds);
    ev_fd_init(&ev_fd[0]);
    ev_fd_init(&ev_fd[1]);

    ev_fd[0].fd = pipefd[0];
    ev_fd[0].events = EV_READ;
    ev_fd[0].hndl = pipein_hndl;

    ev_fd[1].fd = pipefd[1];
    ev_fd[1].events = EV_WRITE;
    ev_fd[1].hndl = pipeout_hndl;

    __ev_fdset_ctl(&fds, EV_ADD, &ev_fd[0]);
    __ev_fdset_ctl(&fds, EV_ADD, &ev_fd[1]);

    ev_fdset_poll(&fds, 10);
    BUG_ON(ev_in_size != 0 || ev_out_size != 1);

    BUG_ON(1 != write(pipefd[1], "1", 1));
    ev_fdset_poll(&fds, 10);
    BUG_ON(ev_in_size != 1 || ev_out_size != 2);

    ev_fdset_poll(&fds, 10);
    BUG_ON(ev_in_size != 2 || ev_out_size != 3);

    __ev_fdset_ctl(&fds, EV_DEL, &ev_fd[0]);
    ev_fdset_poll(&fds, 10);
    BUG_ON(ev_in_size != 2 || ev_out_size != 4);

    ev_fdset_term(&fds);
    return 0;
}

int main(int argc, char** argv) {
    ev_test();
}
