#include <os/alloc.h>
#include <x/xsock.h>
#include <os/timesz.h>
#include "ep_fd.h"

struct fd *fd_new() {
    struct fd *f = (struct fd *)mem_zalloc(sizeof(*f));
    if (f) {
	f->fok = true;
	f->xd = -1;
	INIT_LIST_HEAD(&f->mq);
	INIT_LIST_HEAD(&f->link);
	f->rb_link.key = (char *)f->st.ud;
	f->rb_link.keylen = sizeof(f->st.ud);
    }
    return f;
}

void fd_free(struct fd *f) {
    struct ep_hdr *h, *nh;

    BUG_ON(attached(&f->link));
    if (f->xd >= 0)
	xclose(f->xd);
    list_for_each_eh(h, nh, &f->mq) {
	list_del_init(&h->u.link);
	xfreemsg((char *)h);
    }
    mem_free(f, sizeof(*f));
}

struct xg *xg_new() {
    struct xg *g = (struct xg *)mem_zalloc(sizeof(*g));
    if (g) {
	ssmap_init(&g->fdmap);
	g->rb_link.key = g->group;
	INIT_LIST_HEAD(&g->rcver_head);
	INIT_LIST_HEAD(&g->snder_head);
	INIT_LIST_HEAD(&g->link);
    }
    return g;
}

void xg_free(struct xg *g) {
    struct fd *f, *nf;

    list_for_each_fd(f, nf, &g->rcver_head) {
	xg_rm(g, f);
	fd_free(f);
    }
    list_for_each_fd(f, nf, &g->snder_head) {
	xg_rm(g, f);
	fd_free(f);
    }
    mem_free(g, sizeof(*g));
}

static int xg_empty(struct xg *g) {
    return ssmap_empty(&g->fdmap);
}


struct fd *xg_find(struct xg *g, uuid_t ud) {
    ssmap_node_t *node;

    if ((node = ssmap_find(&g->fdmap, (char *)ud, sizeof(uuid_t))))
	return cont_of(node, struct fd, rb_link);
    return 0;
}

void xg_add(struct xg *g, struct fd *f) {
    switch (f->st.type) {
    case RECEIVER:
	g->rsz++;
	list_add(&f->link, &g->rcver_head);
	break;
    case DISPATCHER:
	g->ssz++;
	list_add(&f->link, &g->snder_head);
	break;
    default:
	BUG_ON(1);
    }
    ssmap_insert(&g->fdmap, &f->rb_link);
}

void xg_rm(struct xg *g, struct fd *f) {
    BUG_ON(!attached(&f->link));
    switch (f->st.type) {
    case RECEIVER:
	g->rsz--;
	break;
    case DISPATCHER:
	g->ssz--;
	break;
    default:
	BUG_ON(1);
    }
    list_del_init(&f->link);
    ssmap_delete(&g->fdmap, &f->rb_link);
}

struct fd *xg_rrbin_go(struct xg *g) {
    struct fd *f;

    if (g->ssz <= 0)
	return 0;
    f = list_first(&g->snder_head, struct fd, link);
    list_move_tail(&f->link, &g->snder_head);
    return f;
}

struct fd *xg_route_back(struct xg *g, uuid_t ud) {
    return xg_find(g, ud);
}



void rtb_init(struct rtb *tb) {
    tb->size = 0;
    INIT_LIST_HEAD(&tb->g_head);
    ssmap_init(&tb->g_map);
}


struct rtb *rtb_new() {
    struct rtb *tb = (struct rtb *)mem_zalloc(sizeof(*tb));
    if (tb)
	rtb_init(tb);
    return tb;
}

static void rtb_rm(struct rtb *tb, struct xg *g);

void rtb_destroy(struct rtb *tb) {
    struct xg *g, *ng;

    list_for_each_xg(g, ng, &tb->g_head) {
	rtb_rm(tb, g);
	xg_free(g);
    }
}

void rtb_free(struct rtb *tb) {
    rtb_destroy(tb);
    mem_free(tb, sizeof(*tb));
}

static struct xg *rtb_find(struct rtb *tb, char *group) {
    ssmap_node_t *node;

    if ((node = ssmap_find(&tb->g_map, group, strlen(group)))) {
	return cont_of(node, struct xg, rb_link);
    }
    return 0;
}


static void rtb_add(struct rtb *tb, struct xg *g) {
    BUG_ON(rtb_find(tb, g->group));
    tb->size++;
    list_add(&tb->g_head, &g->link);
    ssmap_insert(&tb->g_map, &g->rb_link);
}

static void rtb_rm(struct rtb *tb, struct xg *g) {
    BUG_ON(!attached(&g->link));
    tb->size--;
    list_del_init(&g->link);
    ssmap_delete(&tb->g_map, &g->rb_link);
}

int rtb_mapfd(struct rtb *tb, struct fd *f) {
    struct xg *g;

    /* If group doesn't exist, we create it */
    if (!(g = rtb_find(tb, f->st.group))) {
	g = xg_new();
	strcpy(g->group, f->st.group);
	g->rb_link.keylen = strlen(f->st.group);
	rtb_add(tb, g);
    }
    if (xg_find(g, f->st.ud)) {
	errno = EEXIST;
	return -1;
    }
    f->g = g;
    xg_add(g, f);
    return 0;
}

int rtb_unmapfd(struct rtb *tb, struct fd *f) {
    struct xg *g = f->g;

    xg_rm(g, f);
    if (xg_empty(g)) {
	rtb_rm(tb, g);
	xg_free(g);
    }
    return 0;
}

struct fd *rtb_rrbin_go(struct rtb *tb) {
    struct xg *g;

    if (list_empty(&tb->g_head))
	return 0;
    g = list_first(&tb->g_head, struct xg, link);
    list_move_tail(&g->link, &tb->g_head);
    return xg_rrbin_go(g);
}


struct fd *rtb_route_back(struct rtb *tb, uuid_t ud) {
    struct fd *f;
    struct xg *g, *ng;

    list_for_each_xg(g, ng, &tb->g_head) {
	if ((f = xg_route_back(g, ud)))
	    return f;
    }
    return 0;
}
