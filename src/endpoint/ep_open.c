#include <stdio.h>
#include "ep_struct.h"


int xep_open() {
    return efd_alloc();
}
