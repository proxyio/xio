#include "base.h"

extern void global_channel_init();
extern void global_channel_exit();
extern void global_transport_init();
extern void global_transport_exit();



void base_init() {
    global_channel_init();
    global_transport_init();
}


void base_exit() {
    global_channel_exit();
    global_transport_exit();
}
