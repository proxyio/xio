#ifndef _HPIO_ERRNO_
#define _HPIO_ERRNO_

#include <errno.h>

enum {
    PIO_ERRNO                                = 200,
    PIO_EROUTE,
    PIO_ECHKSUM,
    PIO_ETIMEOUT,
    PIO_EREGISTRY,
    PIO_ESERVERDOWN,
    PIO_EINTERN,
    PIO_EQUEUEFULL,
    PIO_ENODISPATCHER,
    PIO_ERRNOEND,
};


#endif
