#include "base.h"

extern void xmodule_init();
extern void xmodule_exit();
extern void global_transport_init();
extern void global_transport_exit();



void base_init() {
    xmodule_init();
    global_transport_init();
}


void base_exit() {
    global_transport_exit();
    xmodule_exit();
}
