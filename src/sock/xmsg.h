#ifndef _HPIO_XMSG_
#define _HPIO_XMSG_

#include <base.h>
#include <ds/list.h>

/*
  The transport protocol header is 6 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    size    |    chunk   |
  +--------+------------+------------+
*/

struct xiov {
    u16 checksum;
    u32 size;
    char chunk[0];
};

struct xmsg {
    struct list_head item;
    struct xiov vec;
};

u32 xiov_len(char *xbuf);
char *xiov_base(char *xbuf);

#define xmsg_walk_safe(pos, next, head)				\
    list_for_each_entry_safe(pos, next, head, struct xmsg,	\
			     item)





#endif
