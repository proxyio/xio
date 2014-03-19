#include <time.h>
#include <sys/time.h>
#include "timesz.h"

int64_t rt_mstime() {
    struct timeval tv;
    int64_t ct;

    gettimeofday(&tv, NULL);
    ct = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    return ct;
}

int64_t rt_ustime() {
    struct timeval tv;
    int64_t ct;

    gettimeofday(&tv, NULL);
    ct = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;

    return ct;
}

int64_t rt_nstime() {
    struct timeval tv;
    int64_t ct;

    gettimeofday(&tv, NULL);
    ct = (int64_t)tv.tv_sec * 1000000000 + (int64_t)tv.tv_usec * 1000;

    return ct;
}
    

int rt_usleep(int64_t usec) {
    struct timespec tv;
    tv.tv_sec = usec / 1000000;
    tv.tv_nsec = (usec % 1000000) * 1000;
    return nanosleep(&tv, NULL);
}
