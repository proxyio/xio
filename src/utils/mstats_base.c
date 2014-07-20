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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "mstats_base.h"

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

static void trigger_one_key_stat (struct mstats_base *ms, i64 nowtime)
{
	int sl, key;
	i64 min_val, max_val, avg_val, all_val;

	min_val = max_val = avg_val = all_val = 0;
	for (sl = MSL_S; sl < MSL_NUM; sl++) {
		if (nowtime - ms->timestamp[sl] <= ms->level[sl])
			continue;
		ms->trigger_counter[sl]++;
		ms->timestamp[sl] = nowtime;
		for (key = 0; key < ms->kr; key++) {
			all_val = ms->keys[MST_NOW][sl][key];
			min_val = ms->keys[MST_MIN][sl][key];
			max_val = ms->keys[MST_MAX][sl][key];
			avg_val = ms->keys[MST_AVG][sl][key];
			if (ms->f[sl] && all_val > ms->thres[sl][key])
				ms->f[sl] (ms, sl, key, ms->thres[sl][key], all_val, min_val, max_val, avg_val);
			ms->keys[MST_LAST][sl][key] = all_val;
			ms->keys[MST_NOW][sl][key] = 0;
		}
	}
}

void mstats_base_emit (struct mstats_base *ms, i64 timestamp)
{
	trigger_one_key_stat (ms, timestamp);
}



int mstats_base_parse (const char *str, const char *key, int *tr, int *v)
{
	char *pos = NULL, *newstr = NULL;

	if (! (pos = (char *) strstr (str, key)) || pos + strlen (key) + 4 > str + strlen (str))
		return -1;
	pos += strlen (key) + 1;
	switch (pos[0]) {
	case 's':
		*tr = MSL_S;
		break;
	case 'm':
		*tr = MSL_M;
		break;
	case 'h':
		*tr = MSL_H;
		break;
	case 'd':
		*tr = MSL_D;
		break;
	default:
		return -1;
	}
	pos += 2;
	newstr = strdup (pos);
	pos = newstr;
	while (pos < newstr + strlen (newstr)) {
		if (pos[0] == ';') {
			pos[0] = '\0';
			break;
		}
		pos++;
	}
	if ((*v = atoi (newstr)) <= 0)
		*v = 1;
	free (newstr);
	return 0;
}
