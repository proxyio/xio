#ifndef TESTUTIL_H_INCLUDED
#define TESTUTIL_H_INCLUDED

static inline int randstr(char *buf, int len)
{
    int i, idx;
    char token[] = "qwertyuioplkjhgfdsazxcvbnm1234567890";
    for (i = 0; i < len; i++) {
        idx = rand() % strlen(token);
        buf[i] = token[idx];
    }
    return 0;
}

#endif
