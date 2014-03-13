#ifndef _HPIO_BIO_
#define _HPIO_BIO_

#include <inttypes.h>
#include "base.h"
#include "ds/list.h"

typedef struct bio_page {
    uint64_t start;
    uint64_t end;
    char page[PAGE_SIZE];
    struct list_head page_link;
} bio_page_t;


struct bio {
    uint64_t bsize;
    int64_t pno;
    struct list_head page_head;
};

struct bio *bio_new();
static inline void bio_init(struct bio *b) {
    INIT_LIST_HEAD(&b->page_head);
}

void bio_destroy(struct bio *b);
void bio_reset(struct bio *b);
int bio_empty(struct bio *b);
int64_t bio_copy(struct bio *b, char *buff, int64_t sz);
int64_t bio_read(struct bio *b, char *buff, int64_t sz);
int64_t bio_write(struct bio *b, const char *buff, int64_t sz);

int64_t bio_flush(struct bio *b, struct io *io_ops);
int64_t bio_prefetch(struct bio *b, struct io *io_ops);


#endif
