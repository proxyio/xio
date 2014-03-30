#include <gtest/gtest.h>
#include <sys/signal.h>
extern "C" {
#include "base/base.h"
}

int gargc = 0;
char **gargv = NULL;

int randstr(char *buf, int len) {
    int i, idx;
    char token[] = "qwertyuioplkjhgfdsazxcvbnm1234567890";
    for (i = 0; i < len; i++) {
	idx = rand() % strlen(token);
	buf[i] = token[idx];
    }
    return 0;
}

int main(int argc, char **argv) {

    int ret;
    
    gargc = argc;
    gargv = argv;

    base_init();
    // Ignore SIGPIPE
    if (SIG_ERR == signal(SIGPIPE, SIG_IGN)) {
        fprintf(stderr, "signal SIG_IGN");
        return -1;
    }
    testing::InitGoogleTest(&gargc, gargv);
    ret = RUN_ALL_TESTS();
    base_exit();
    return ret;
}
