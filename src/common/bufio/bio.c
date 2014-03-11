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


bio_page_t *bio_page_new() {
    bio_page_t *bp = (bio_page_t *)mem_zalloc(sizeof(*bp));
    return bp;
}

struct bio *bio_new() {
    struct bio *b = (struct bio *)mem_zalloc(sizeof(*b));
    return b;
}

void bio_destroy(struct bio *b) {
    bio_page_t *bp, *tmp;
    list_for_each_page_safe(bp, tmp, &b->page_head) {
	list_del(&bp->page_link);
	mem_free(bp, sizeof(*bp));
    }
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

int64_t bio_readfull(struct bio *b, char *buff, int64_t sz) {
    bio_page_t *bp, *tmp;
    int64_t pno = 0;
    
    if (sz < b->bsize)
	return -1;
    list_for_each_page_safe(bp, tmp, &b->page_head) {
	memcpy(buff + pno * PAGE_SIZE, bp->page,
	       pno < b->pno ? PAGE_SIZE : b->bsize % PAGE_SIZE);
	pno++;
    }
    return b->bsize;
}

static inline int is_time_to_expand(struct bio *b) {
    if (list_empty(&b->page_head) || (b->bsize % PAGE_SIZE) == 0)
	return true;
    return false;
}


int64_t bio_write(struct bio *b, const char *buff, int64_t sz) {
    bio_page_t *bp;
    int64_t done = 0, orr, nbytes;

    while (done < sz) {
	if (is_time_to_expand(b) && bio_expand_one_page(b) < 0)
	    break;
	bp = bio_last(b);
	orr = b->bsize % PAGE_SIZE;
	nbytes = sz > PAGE_SIZE - orr ? PAGE_SIZE - orr : sz;
	memcpy(bp->page + orr, buff + done, nbytes);
	done += nbytes;
	b->bsize += nbytes;
    }
    return done;
}
