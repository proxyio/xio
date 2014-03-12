#include "bio.h"
#include "os/malloc.h"

#define list_for_each_page_safe(bp, tmp, head)				\
    list_for_each_entry_safe(bp, tmp, head, bio_page_t, page_link)

static inline bio_page_t *bio_first(struct bio *b) {
    bio_page_t *bp = NULL;
    if (!list_empty(&b->page_head))
	bp = list_first(&b->page_head, bio_page_t, page_link);
    return bp;
}

static inline bio_page_t *bio_last(struct bio *b) {
    bio_page_t *bp = NULL;
    if (!list_empty(&b->page_head))
	bp = list_last(&b->page_head, bio_page_t, page_link);
    return bp;
}


static inline bio_page_t *bio_page_new() {
    bio_page_t *bp = (bio_page_t *)mem_zalloc(sizeof(*bp));
    return bp;
}

static inline int64_t
bio_page_read(bio_page_t *bp, char *buff, int64_t sz) {
    sz = (bp->end - bp->start) > sz ? sz : bp->end - bp->start;
    memcpy(buff, bp->page + bp->start, sz);
    bp->start += sz;
    memmove(bp->page, bp->page + bp->start, bp->end - bp->start);
    return sz;
}

static inline int64_t
bio_page_write(bio_page_t *bp, const char *buff, int64_t sz) {
    sz = (PAGE_SIZE - (bp->end - bp->start)) > sz ?
	sz : (PAGE_SIZE - (bp->end - bp->start));
    memcpy(bp->page + bp->end, buff, sz);
    bp->end += sz;
    return sz;
}



struct bio *bio_new() {
    struct bio *b = (struct bio *)mem_zalloc(sizeof(*b));
    return b;
}

static inline void __bio_cleanup_pages(struct bio *b) {
    bio_page_t *bp, *tmp;
    list_for_each_page_safe(bp, tmp, &b->page_head) {
	list_del(&bp->page_link);
	mem_free(bp, sizeof(*bp));
    }
}


void bio_destroy(struct bio *b) {
    __bio_cleanup_pages(b);
}

static inline int bio_expand_one_page(struct bio *b) {
    bio_page_t *bp = bio_page_new();
    if (!bp)
	return -1;
    b->pno++;
    list_add_tail(&bp->page_link, &b->page_head);
    return 0;
}

static inline int bio_shrink_one_page(struct bio *b) {
    bio_page_t *bp;

    if (list_empty(&b->page_head))
	return -1;
    bp = bio_last(b);
    list_del(&bp->page_link);
    b->pno--;
    mem_free(bp, sizeof(*bp));
    return 0;
}

int bio_prefetch(struct bio *b) {
    int64_t nbytes;
    char page[PAGE_SIZE];
    struct io *ops = &b->io_ops;
    
    while ((nbytes = ops->read(ops, page, PAGE_SIZE)) > 0)
	BUG_ON(bio_write(b, page, nbytes) != nbytes);
    return 0;
}

int64_t bio_fetch(struct bio *b, char *buff, int64_t sz) {
    bio_page_t *bp, *tmp;
    int64_t pno = 0;

    list_for_each_page_safe(bp, tmp, &b->page_head) {
	memcpy(buff + pno * PAGE_SIZE, bp->page,
	       pno < b->pno ? PAGE_SIZE : b->bsize % PAGE_SIZE);
	pno++;
    }
    __bio_cleanup_pages(b);
    return b->bsize;
}

int64_t bio_readfull(struct bio *b, char *buff, int64_t sz) {
    bio_page_t *bp, *tmp;
    int64_t nbytes = 0;

    list_for_each_page_safe(bp, tmp, &b->page_head) {
	nbytes += bio_page_read(bp, buff + nbytes, sz);
    }
    __bio_cleanup_pages(b);
    return nbytes;
}

static inline int is_time_to_expand(struct bio *b) {
    if (list_empty(&b->page_head) || (b->bsize % PAGE_SIZE) == 0)
	return true;
    return false;
}


int64_t bio_write(struct bio *b, const char *buff, int64_t sz) {
    bio_page_t *bp;
    int64_t nbytes = 0;

    while (nbytes < sz) {
	if (is_time_to_expand(b) && bio_expand_one_page(b) < 0)
	    break;
	bp = bio_last(b);
	nbytes += bio_page_write(bp, buff + nbytes, sz);
    }
    b->bsize += nbytes;
    return nbytes;
}


int64_t bio_flush(struct bio *b) {
    return -1;
}
