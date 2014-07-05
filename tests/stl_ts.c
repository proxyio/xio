#include <stdio.h>
#include <utils/base.h>
#include <utils/str_array.h>

struct flagset {
	union {
		u32 closed:1;
		u32 shutdown:1;
		u32 bad;
	};
};

static void test_str_array ()
{
	int i;
	struct str_array arr = {};
	char *hs = "tcp://127.0.0.1:1880+inproc://xxxxx+ipc://tmp/sock.s";
	char *needle = "+";

	str_array_init (&arr);
	str_split (hs, &arr, needle);
	BUG_ON (arr.size != 3);
	BUG_ON (strcmp (arr.at[0], "tcp://127.0.0.1:1880") != 0);
	BUG_ON (strcmp (arr.at[1], "inproc://xxxxx") != 0);
	BUG_ON (strcmp (arr.at[2], "ipc://tmp/sock.s") != 0);
	str_array_destroy (&arr);
}



int main (int argc, char **argv)
{
	struct flagset fs = {};

	BUG_ON(fs.bad);

	fs.closed = 1;
	BUG_ON(!fs.bad);

	fs.closed = 0;
	BUG_ON(fs.bad);
	fs.shutdown = 1;
	BUG_ON(!fs.bad);

	test_str_array ();
}
