#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_


struct transport {
    const char *name;
    int fd;
    void (*init) (struct transport *tp);
    void (*destroy) (struct transport *tp);
    int (*bind) (struct transport *tp, const char *addr);
    int (*connect) (struct transport *tp, const char *addr);
    int (*setopt) (struct transport *tp, int option, const void *optval,
		   int optvallen);
    int (*getopt) (struct nn_optset *self, int option, void *optval,
		   int *optvallen);
};




#endif
