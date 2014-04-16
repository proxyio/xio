#include "base.h"

extern void global_xinit();
extern void global_xexit();
extern void global_transport_init();
extern void global_transport_exit();



void base_init() {
    global_xinit();
    global_transport_init();
}


void base_exit() {
    global_xexit();
    global_transport_exit();
}
