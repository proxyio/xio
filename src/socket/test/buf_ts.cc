#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
#include <xio/poll.h>
#include <xio/socket.h>
}

using namespace std;
extern int randstr(char *buf, int len);


TEST(xmsg, outofband) {
    int oob_count = -1;
    struct xcmsg ent = {};
    char *xbuf = xallocmsg(12);
    char *xbuf2;
    char oob1[12];
    char oob2[12];
    char oob3[12];


    BUG_ON(xmsgctl(xbuf, XMSG_CLONE, &xbuf2));
    BUG_ON(xmsgctl(xbuf2, XMSG_OOBNUM, &oob_count));
    BUG_ON(oob_count != 0);
    xfreemsg(xbuf2);
    
    randstr(xbuf, 12);
    randstr(oob1, sizeof(oob1));
    randstr(oob2, sizeof(oob2));
    randstr(oob3, sizeof(oob3));

    ent.idx = 0;
    ent.outofband = xallocmsg(sizeof(oob1));
    memcpy(ent.outofband, oob1, sizeof(oob1));
    BUG_ON(xmsgctl(xbuf, XMSG_SETOOB, &ent) != 0);

    ent.idx = 2;
    ent.outofband = xallocmsg(sizeof(oob3));
    memcpy(ent.outofband, oob3, sizeof(oob3));
    BUG_ON(xmsgctl(xbuf, XMSG_SETOOB, &ent) != 0);

    ent.idx = 1;
    ent.outofband = xallocmsg(sizeof(oob2));
    memcpy(ent.outofband, oob2, sizeof(oob2));
    BUG_ON(xmsgctl(xbuf, XMSG_SETOOB, &ent) != 0);



    BUG_ON(xmsgctl(xbuf, XMSG_CLONE, &xbuf2));
    BUG_ON(xmsgctl(xbuf2, XMSG_OOBNUM, &oob_count));
    BUG_ON(oob_count != 3);
    
    ent.idx = 2;
    BUG_ON(xmsgctl(xbuf, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob3, sizeof(oob3)) != 0);
    BUG_ON(xmsgctl(xbuf2, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob3, sizeof(oob3)) != 0);

    
    ent.idx = 1;
    BUG_ON(xmsgctl(xbuf, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob2, sizeof(oob2)) != 0);
    BUG_ON(xmsgctl(xbuf2, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob2, sizeof(oob2)) != 0);

    ent.idx = 0;
    BUG_ON(xmsgctl(xbuf, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob1, sizeof(oob1)) != 0);
    BUG_ON(xmsgctl(xbuf2, XMSG_GETOOB, &ent) != 0);
    BUG_ON(memcmp(ent.outofband, oob1, sizeof(oob1)) != 0);

    xfreemsg(xbuf);
    xfreemsg(xbuf2);
}

