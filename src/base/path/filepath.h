#ifndef _HPIO_FILEPATH_
#define _HPIO_FILEPATH_

#include "os/alloc.h"

char *fp_join(const char *p1, const char *p2);
char *fp_abs(const char *path);
char *fp_base(const char *path);
char *fp_dir(const char *path);
int fp_isabs(const char *path);
int fp_hasprefix(const char *path, const char *prefix);
int fp_hassuffix(const char *path, const char *suffix);

typedef void (*walkFn)(const char *path, void *data);

typedef struct filepath {
    char *root;
    int mask, cur_deep, max_deep;
} filepath_t;

static inline filepath_t *filepath_new() {
    filepath_t *fp = (filepath_t *)mem_zalloc(sizeof(*fp));
    return fp;
}

static inline int filepath_init(filepath_t *fp, const char *root) {
    const char *cr = ".";

    if (strlen(root) == 0 || !root)
	root = cr;
    if (!(fp->root = strdup(root)))
	return -1;
    return 0;
}

static inline void filepath_destroy(filepath_t *fp) {
    if (fp->root)
	mem_free(fp->root, strlen(fp->root));
}

enum {
    W_FILE = 1,
    W_DIR = 2,
};

int filepath_walk(filepath_t *fp, walkFn f, void *args, int mask, int deep);

static inline int fp_walk(filepath_t *fp, walkFn f, void *args) {
    return filepath_walk(fp, f, args, W_FILE|W_DIR, 1);
}

static inline int fp_dwalk(filepath_t *fp, walkFn f, void *args, int deep) {
    return filepath_walk(fp, f, args, W_FILE|W_DIR, deep);
}


static inline int fp_walkfile(filepath_t *fp, walkFn f, void *args) {
    return filepath_walk(fp, f, args, W_FILE, 1);
}

static inline int fp_dwalkfile(filepath_t *fp, walkFn f, void *args, int deep) {
    return filepath_walk(fp, f, args, W_FILE, deep);
}


static inline int fp_walkdir(filepath_t *fp, walkFn f, void *args) {
    return filepath_walk(fp, f, args, W_DIR, 1);
}

static inline int fp_dwalkdir(filepath_t *fp, walkFn f, void *args, int deep) {
    return filepath_walk(fp, f, args, W_DIR, deep);
}






#endif
