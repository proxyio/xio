#include <utils/modstat.h>

static void s_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val)
{
}
static void m_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val)
{
}
static void h_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val)
{
}
static void d_warn(modstat_t *self, int sl, int key, int64_t ts, int64_t val)
{
}

enum {
    SEND = 0,
    RECV,
    UT_KEYRANGE,
};

DEFINE_MODSTAT(ut, UT_KEYRANGE);


static int modstat_test()
{
	int i;
	ut_modstat_t utms = {};
	modstat_t *ms = &utms.self;

	INIT_MODSTAT(utms);
	ms->slv[MSL_S] = 500;
	ms->slv[MSL_M] = 1000;
	ms->slv[MSL_H] = 2000;
	ms->slv[MSL_D] = 4000;
	modstat_set_warnf(ms, MSL_S, s_warn);
	modstat_set_warnf(ms, MSL_M, m_warn);
	modstat_set_warnf(ms, MSL_H, h_warn);
	modstat_set_warnf(ms, MSL_D, d_warn);
	modstat_set_threshold(ms, MSL_S, SEND, 100);
	modstat_set_threshold(ms, MSL_M, SEND, 200);
	modstat_set_threshold(ms, MSL_H, SEND, 300);
	modstat_set_threshold(ms, MSL_D, SEND, 500);
	for (i = 0; i < 1000; i++) {
		modstat_incrkey(ms, SEND);
		modstat_incrkey(ms, RECV);
		modstat_update_timestamp(ms, gettimeof(ms));
	}
	return 0;
}


static int gtip(const char *str, const char *item, int *tr, int *v)
{
	return generic_parse_modstat_item(str, item, tr, v);
}

static int modstat_item_parse_test()
{
	int tr = 0;
	int v = 0;
	BUG_ON(gtip(";;RECONNECT:m:1;SEND_BYTES:m:1;", "RECONNECT", &tr, &v));
	BUG_ON(tr != MSL_M);
	BUG_ON(v != 1);

	BUG_ON(gtip(";;RECONNECT:m:1000;SEND_BYTES:m:1;", "RECONNECT", &tr, &v));
	BUG_ON(tr != MSL_M);
	BUG_ON(v != 1000);

	BUG_ON(gtip("RECONNECT:d:120;SEND_BYTES:m:1;", "RECONNECT", &tr, &v));
	BUG_ON(tr != MSL_D);
	BUG_ON(v != 120);

	BUG_ON(gtip("RECONNECT:s:120", "RECONNECT", &tr, &v));
	BUG_ON(tr != MSL_S);
	BUG_ON(v != 120);

	BUG_ON(gtip("RECONNECT:h:", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("RECONNECT:a:9", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("ECONNECT:m:1", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("RECONNECT:h", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("RECONNECT:12", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("RECONNECTh", "RECONNECT", &tr, &v) != -1);
	BUG_ON(gtip("RECONNECTh:m:12", "RECONNECT", &tr, &v) != -1);
	return 0;
}

int main(int argc, char **argv)
{
	modstat_test();
	modstat_item_parse_test();
	return 0;
}
