#include <errno.h>
#include <time.h>
#include <string.h>
#include <utils/spinlock.h>
#include <utils/thread.h>
#include <xio/poll.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include "testutil.h"

int main(int argc, char **argv)
{
    int oob_count = -1;
    struct xcmsg ent = {};
    char *xbuf = xallocubuf(12);
    char oob1[12];
    char oob2[12];
    char oob3[12];


    randstr(xbuf, 12);
    randstr(oob1, sizeof(oob1));
    randstr(oob2, sizeof(oob2));
    randstr(oob3, sizeof(oob3));

    ent.outofband = xallocubuf(sizeof(oob1));
    memcpy(ent.outofband, oob1, sizeof(oob1));
    BUG_ON(ubufctl(xbuf, UBUF_ADDCMSG, &ent) != 0);

    ent.outofband = xallocubuf(sizeof(oob3));
    memcpy(ent.outofband, oob3, sizeof(oob3));
    BUG_ON(ubufctl(xbuf, UBUF_ADDCMSG, &ent) != 0);

    ent.outofband = xallocubuf(sizeof(oob2));
    memcpy(ent.outofband, oob2, sizeof(oob2));
    BUG_ON(ubufctl(xbuf, UBUF_ADDCMSG, &ent) != 0);

    ent.idx = 2;
    BUG_ON(ubufctl(xbuf, UBUF_GETCMSG, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob2, sizeof(oob2)) != 0);

    ent.idx = 1;
    BUG_ON(ubufctl(xbuf, UBUF_GETCMSG, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob3, sizeof(oob3)) != 0);

    ent.idx = 0;
    BUG_ON(ubufctl(xbuf, UBUF_GETCMSG, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob1, sizeof(oob1)) != 0);

    xfreeubuf(xbuf);
    return 0;
}

