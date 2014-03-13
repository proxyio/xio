#ifndef _HPIO_PID_
#define _HPIO_PID_

#include <string.h>
#include "os/memory.h"


typedef struct pctl {
    char *exe, *name, *root;
    char *pidf;
} pctl_t;


static inline pctl_t *pctl_new(const char *exe, const char *name, const char *root) {
    pctl_t *pt = (pctl_t *)mem_zalloc(sizeof(*pt));
    if (pt && (pt->exe = strdup(exe)) && (pt->name = strdup(name)) && (pt->root = strdup(root)))
	return pt;
    if (pt) {
	mem_free(pt->exe, strlen(exe));
	mem_free(pt->name, strlen(name));
	mem_free(pt->root, strlen(root));
    }
    mem_free(pt, sizeof(*pt));
    return NULL;
}

static inline int pctl_free(pctl_t *pt) {
    mem_free(pt->exe, strlen(pt->exe));
    mem_free(pt->name, strlen(pt->name));
    mem_free(pt->root, strlen(pt->root));
    mem_free(pt, sizeof(*pt));
    return 0;
}




#endif
