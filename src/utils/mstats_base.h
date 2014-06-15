/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#ifndef _H_PROXYIO_MSTATS_BASE_
#define _H_PROXYIO_MSTATS_BASE_

#include "base.h"
#include "timer.h"

#define MSTATS_BASE_KEYMAX 256

struct mstats_base;
typedef void (*thres_warn)
(struct mstats_base *stb, int sl, int key, int64_t thres, int64_t val);

enum {
	MST_NOW = 0,
	MST_LAST,
	MST_MIN,
	MST_MAX,
	MST_AVG,
	MST_NUM,
};

enum {
	MSL_A = 0,
	MSL_S,
	MSL_M,
	MSL_H,
	MSL_D,
	MSL_NUM,
};

struct mstats_base {
	int kr;
	int64_t slv[MSL_NUM];
	int64_t *keys[MST_NUM][MSL_NUM];
	int64_t *thres[MSL_NUM];
	int64_t timestamp[MSL_NUM];
	int64_t trigger_counter[MSL_NUM];
	thres_warn *f;
};



#define DEFINE_MSTATS(name, KEYRANGE)					\
	struct name##_mstats {						\
		int64_t keys[MST_NUM][MSL_NUM][KEYRANGE];		\
		int64_t thres[MSL_NUM][KEYRANGE];			\
		thres_warn f[MSL_NUM];					\
		struct mstats_base base;				\
	};								\
	static inline							\
	void name##_mstats_init (struct name##_mstats *st)		\
	{								\
		int i, j;						\
		struct mstats_base *stb = &st->base;			\
		ZERO(*st);						\
		stb->kr = KEYRANGE;					\
		stb->slv[MSL_S] = 1000;					\
		stb->slv[MSL_M] = 60000;				\
		stb->slv[MSL_H] = 3600000;				\
		stb->slv[MSL_D] = 86400000;				\
		foreach (i, MST_NUM) foreach (j, MSL_NUM)		\
			stb->keys[i][j] = (int64_t *)st->keys[i][j];	\
		foreach (i, MSL_NUM)					\
			stb->thres[i] = (int64_t *)st->thres[i];	\
		stb->f = (thres_warn *)st->f;				\
		foreach (i, MSL_NUM)					\
			stb->timestamp[i] = gettimeof(ms);		\
	}

static inline void mstats_base_incrkey (struct mstats_base *stb, int key)
{
	int sl;
	for (sl = 0; sl < MSL_NUM; sl++)
		stb->keys[MST_NOW][sl][key] += 1;
}

static inline void mstats_base_incrskey (struct mstats_base *stb, int key, int val)
{
	int sl;
	for (sl = 0; sl < MSL_NUM; sl++)
		stb->keys[MST_NOW][sl][key] += val;
}

static inline int64_t mstats_base_getkey (struct mstats_base *stb, int st, int sl, int key)
{
	return stb->keys[st][sl][key];
}

static inline void mstats_base_set_warnf (struct mstats_base *stb, int sl, thres_warn f)
{
	stb->f[sl] = f;
}

static inline void mstats_base_set_thres (struct mstats_base *stb, int sl, int key, int64_t v)
{
	stb->thres[sl][key] = v;
}

void mstats_base_emit (struct mstats_base *stb, int64_t timestamp);
int mstats_base_parse (const char *str, const char *key, int *tr, int *v);

static inline int mstats_base_init (struct mstats_base *stb, int keyrange,
				    const char **items, const char *ss)
{
	int i = 0, sl = 0, val = 0;
	for (i = 1; i < keyrange; i++)
		if ( (mstats_base_parse (ss, items[i], &sl, &val) ) == 0)
			mstats_base_set_thres (stb, sl, i, val);
	return 0;
}

#endif
