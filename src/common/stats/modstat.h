#ifndef _HPIO_MODULESTAT_
#define _HPIO_MODULESTAT_

#include "base.h"
#include "os/timestamp.h"

#define MODSTAT_KEYMAX 256

struct modstat;
typedef void (*threshold_warn)
(struct modstat *self, int sl, int key, int64_t threshold, int64_t val);

enum STAT_TYPE {
    MST_NOW = 0,
    MST_LAST,
    MST_MIN,
    MST_MAX,
    MST_AVG,
    MST_NUM,
};
extern const char *stat_type_token[MST_NUM];

enum STAT_LEVEL {
    MSL_A = 0,
    MSL_S,
    MSL_M,
    MSL_H,
    MSL_D,
    MSL_NUM,
};

extern const char *stat_level_token[MSL_NUM];

typedef struct modstat {
    int kr;
    int64_t slv[MSL_NUM];
    int64_t *keys[MST_NUM][MSL_NUM];
    int64_t *threshold[MSL_NUM];
    int64_t timestamp[MSL_NUM];
    int64_t trigger_counter[MSL_NUM];
    threshold_warn *f;
} modstat_t;


#define DEFINE_MODSTAT(name, KEYRANGE)			\
    typedef struct name##_modstat {			\
	int64_t keys[MST_NUM][MSL_NUM][KEYRANGE];	\
	int64_t threshold[MSL_NUM][KEYRANGE];		\
	threshold_warn f[MSL_NUM];			\
	modstat_t self;					\
    } name##_modstat_t


#define INIT_MODSTAT(ms) do {						\
	int sl, st;							\
	modstat_t *self = &ms.self;					\
	ZERO(ms);							\
	self->kr = sizeof(ms.keys) /					\
	    (MSL_NUM * MST_NUM * sizeof(int64_t));			\
	self->slv[MSL_S] = 1000;					\
	self->slv[MSL_M] = 60000;					\
	self->slv[MSL_H] = 3600000;					\
	self->slv[MSL_D] = 86400000;					\
	for (st = 0; st < MST_NUM; st++)				\
	    for (sl = 0; sl < MSL_NUM; sl++)				\
		self->keys[st][sl] = (int64_t *)&ms.keys[st][sl];	\
	for (sl = 0; sl < MSL_NUM; sl++)				\
	    self->threshold[sl] = (int64_t *)&ms.threshold[sl];		\
	self->f = (threshold_warn *)&ms.f;				\
	for (sl = 0; sl < MSL_NUM; sl++)				\
	    self->timestamp[sl] = rt_mstime();				\
    } while (0)

static inline void modstat_incrkey(modstat_t *ms, int key) {
    int sl;
    for (sl = 0; sl < MSL_NUM; sl++)
	ms->keys[MST_NOW][sl][key] += 1;
}

static inline void modstat_incrskey(modstat_t *ms, int key, int val) {
    int sl;
    for (sl = 0; sl < MSL_NUM; sl++)
	ms->keys[MST_NOW][sl][key] += val;
}

static inline int64_t modstat_getkey(modstat_t *ms, int st, int sl, int key) {
    return ms->keys[st][sl][key];
}

static inline void modstat_set_warnf(modstat_t *ms, int sl, threshold_warn f) {
    ms->f[sl] = f;
}

static inline void modstat_set_threshold(modstat_t *ms, int sl, int key, int64_t v) {
    ms->threshold[sl][key] = v;
}

void modstat_update_timestamp(modstat_t *ms, int64_t timestamp);
int generic_parse_modstat_item(const char *str, const char *key, int *tr, int *v);
static inline int generic_init_modstat(modstat_t *self, int keyrange,
				       const char **items, const char *ss) {
    int i = 0, sl = 0, val = 0;
    for (i = 1; i < keyrange; i++)
	if ((generic_parse_modstat_item(ss, items[i], &sl, &val)) == 0)
	    modstat_set_threshold(self, sl, i, val);
    return 0;
}

#endif
