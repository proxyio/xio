#include <utils/mstats_base.h>

static void s_warn (struct mstats_base *stb, int sl, int key, i64 ts, i64 val, i64 min_val, i64 max_val, i64 avg_val)
{
}
static void m_warn (struct mstats_base *stb, int sl, int key, i64 ts, i64 val, i64 min_val, i64 max_val, i64 avg_val)
{
}
static void h_warn (struct mstats_base *stb, int sl, int key, i64 ts, i64 val, i64 min_val, i64 max_val, i64 avg_val)
{
}
static void d_warn (struct mstats_base *stb, int sl, int key, i64 ts, i64 val, i64 min_val, i64 max_val, i64 avg_val)
{
}

enum {
    SEND = 0,
    RECV,
    UT_KEYRANGE,
};

DEFINE_MSTATS (ut, UT_KEYRANGE);


static int mstats_test()
{
	int i;
	struct ut_mstats utms = {};
	struct mstats_base *ms = &utms.base;

	ut_mstats_init (&utms);
	ms->level[MSL_S] = 500;
	ms->level[MSL_M] = 1000;
	ms->level[MSL_H] = 2000;
	ms->level[MSL_D] = 4000;
	mstats_base_set_warnf (ms, MSL_S, s_warn);
	mstats_base_set_warnf (ms, MSL_M, m_warn);
	mstats_base_set_warnf (ms, MSL_H, h_warn);
	mstats_base_set_warnf (ms, MSL_D, d_warn);
	mstats_base_set_thres (ms, MSL_S, SEND, 100);
	mstats_base_set_thres (ms, MSL_M, SEND, 200);
	mstats_base_set_thres (ms, MSL_H, SEND, 300);
	mstats_base_set_thres (ms, MSL_D, SEND, 500);
	for (i = 0; i < 1000; i++) {
		mstats_base_incr (ms, SEND);
		mstats_base_incr (ms, RECV);
		mstats_base_emit (ms, gettimeof (ms) );
	}
	return 0;
}


static int gtip (const char *str, const char *item, int *tr, int *v)
{
	return mstats_base_parse (str, item, tr, v);
}

static int mstats_item_parse_test()
{
	int tr = 0;
	int v = 0;
	BUG_ON (gtip (";;RECONNECT:m:1;SEND_BYTES:m:1;", "RECONNECT", &tr, &v) );
	BUG_ON (tr != MSL_M);
	BUG_ON (v != 1);

	BUG_ON (gtip (";;RECONNECT:m:1000;SEND_BYTES:m:1;", "RECONNECT", &tr, &v) );
	BUG_ON (tr != MSL_M);
	BUG_ON (v != 1000);

	BUG_ON (gtip ("RECONNECT:d:120;SEND_BYTES:m:1;", "RECONNECT", &tr, &v) );
	BUG_ON (tr != MSL_D);
	BUG_ON (v != 120);

	BUG_ON (gtip ("RECONNECT:s:120", "RECONNECT", &tr, &v) );
	BUG_ON (tr != MSL_S);
	BUG_ON (v != 120);

	BUG_ON (gtip ("RECONNECT:h:", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("RECONNECT:a:9", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("ECONNECT:m:1", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("RECONNECT:h", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("RECONNECT:12", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("RECONNECTh", "RECONNECT", &tr, &v) != -1);
	BUG_ON (gtip ("RECONNECTh:m:12", "RECONNECT", &tr, &v) != -1);
	return 0;
}

int main (int argc, char **argv)
{
	mstats_test();
	mstats_item_parse_test();
	return 0;
}
