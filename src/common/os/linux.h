#ifndef _LINUX_H_
#define _LINUX_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

static int init_daemon() {
    int fd;
    
    switch (fork()) {
    case -1:
        return -1;
    case 0:
        break;
    default:
        exit(0);
    }

    if (setsid() < 0)
        return -1;
    umask(0);
    if ((fd = open("/dev/null", O_RDWR)) < 0)
        return -1;
    if (dup2(fd, STDIN_FILENO) < 0)
        return -1;
    if (dup2(fd, STDOUT_FILENO) < 0)
        return -1;
    if (fd > STDERR_FILENO && close(fd) < 0)
	return -1;
    return 0;
}

#endif
