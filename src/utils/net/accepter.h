#ifndef _HPIO_ACT_
#define _HPIO_ACT_

enum {
    ACT_BLOCK = 1,
    ACT_REUSEADDR = 2,
};

int act_listen(const char *net, const char *sock, int backlog);
int act_accept(int afd);
int act_setopt(int afd, int optname, ...);

#endif
