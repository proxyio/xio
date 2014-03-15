#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

int randstr(char *buf, int sz) {
    int i, idx;
    char token[] = "qwertyuioplkjhgfdsazxcvbnm1234567890";
    for (i = 0; i < sz; i++) {
	idx = rand() % strlen(token);
	buf[i] = token[idx];
    }
    return 0;
}

extern int pingpong_start(struct bc_opt *cf);
extern int exception_start(struct bc_opt *cf);

int main(int argc, char **argv) {
    struct bc_opt cf = {};

    if (getoption(argc, argv, &cf) < 0)
	return -1;
    switch (cf.mode) {
    case PINGPONG:
	pingpong_start(&cf);
	break;
    case EXCEPTION:
	exception_start(&cf);
	break;
    }
    return 0;
}
