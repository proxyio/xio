#ifndef _BASE_H_
#define _BASE_H_

#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0

#define PATH_MAX 4096
#define ZERO(x) memset(&(x), 0, sizeof(x))
#define ERROR (-1 & __LINE__)
#define STREQ(a, b) (strlen(a) == strlen(b) && memcmp(a, b , strlen(a)) == 0)





#endif
