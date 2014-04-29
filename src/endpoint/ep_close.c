#include <stdio.h>
#include "ep_struct.h"


void xep_close(int efd) {
    efd_free(efd);
}
