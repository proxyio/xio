#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "modstat.h"

const char *stat_type_token[MST_NUM] = {
    "now",
    "last",
    "min",
    "max",
    "avg",
};


const char *stat_level_token[MSL_NUM] = {
    "total",
    "second",
    "minute",
    "hour",
    "day",
};


static void trigger_one_key_stat(modstat_t *ms, int64_t nowtime) {
    int sl, key;
    for (sl = MSL_S; sl < MSL_NUM; sl++) {
	if (nowtime - ms->timestamp[sl] <= ms->slv[sl])
	    continue;
	for (key = 0; key < ms->kr; key++) {
	    if (ms->f[sl] && ms->keys[MST_NOW][sl][key] > ms->threshold[sl][key])
		ms->f[sl](ms, sl, key, ms->threshold[sl][key], ms->keys[MST_NOW][sl][key]);
	    ms->keys[MST_LAST][sl][key] = ms->keys[MST_NOW][sl][key];
	    if (!ms->keys[MST_MIN][sl][key] || ms->keys[MST_NOW][sl][key] < ms->keys[MST_MIN][sl][key])
		ms->keys[MST_MIN][sl][key] = ms->keys[MST_NOW][sl][key];
	    if (!ms->keys[MST_MAX][sl][key] || ms->keys[MST_NOW][sl][key] > ms->keys[MST_MAX][sl][key])
		ms->keys[MST_MAX][sl][key] = ms->keys[MST_NOW][sl][key];
	}
    }
}

static void update_one_key_stat(modstat_t *ms, int64_t nowtime) {
    int sl, key;
    for (sl = MSL_S; sl < MSL_NUM; sl++) {
	if (nowtime - ms->timestamp[sl] <= ms->slv[sl])
	    continue;
	ms->trigger_counter[sl]++;
	ms->timestamp[sl] = nowtime;
	for (key = 0; key < ms->kr; key++)
	    ms->keys[MST_NOW][sl][key] = 0;
    }
}

void modstat_update_timestamp(modstat_t *ms, int64_t timestamp) {
    trigger_one_key_stat(ms, timestamp);
    update_one_key_stat(ms, timestamp);
}



int generic_parse_modstat_item(const char *str, const char *key, int *tr, int *v) {
    char *pos = NULL, *newstr = NULL;
    
    if (!(pos = (char *)strstr(str, key)) || pos + strlen(key) + 4 > str + strlen(str))
	return -1;
    pos += strlen(key) + 1;
    switch (pos[0]) {
    case 's': *tr = MSL_S;
	break;
    case 'm': *tr = MSL_M;
	break;
    case 'h': *tr = MSL_H;
	break;
    case 'd': *tr = MSL_D;
	break;
    default:
	return -1;
    }
    pos += 2;
    newstr = strdup(pos);
    pos = newstr;
    while (pos < newstr + strlen(newstr)) {
	if (pos[0] == ';') {
	    pos[0] = '\0';
	    break;
	}
	pos++;
    }
    if ((*v = atoi(newstr)) <= 0)
	*v = 1;
    free(newstr);
    return 0;
}
