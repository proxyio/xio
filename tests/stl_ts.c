#include <stdio.h>
#include <utils/base.h>

struct flagset {
	union {
		u32 closed:1;
		u32 shutdown:1;
		u32 bad;
	};
};

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
}
