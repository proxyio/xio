#ifndef _XIO_EPHDR_
#define _XIO_EPHDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>
#include <hash/crc.h>

/* The ephdr looks like this:
 * +-------+---------+-------+----------------+
 * | ephdr |  epr[]  |  uhdr |  udata content |
 * +-------+---------+-------+----------------+
 */

struct epr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct ephdr {
    u8 version;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    u32 size;
    u16 timeout;
    i64 sendstamp;
    union {
	u64 __align[2];
	struct list_head link;
    } u;
    struct epr rt[0];
};

struct uhdr {
    u16 ephdr_off;
};

struct ephdr *udata2ephdr(char *udata) {
    struct uhdr *uh = (struct uhdr *)(udata - sizeof(*uh));
    return (struct ephdr *)(udata - uh->ephdr_off);
}



#endif
