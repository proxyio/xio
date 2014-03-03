#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <nspio/errno.h>
#include "net/conn.h"
#include "proto/proto.h"
#include "os/memalloc.h"


NSPIO_DECLARATION_START


proto_parser::proto_parser() :
    dropped_cnt(0), msg_max_size(0), bufhdr(NULL),
    recv_hdr_index(0), recv_data_index(0),
    send_hdr_index(0), send_data_index(0)
{
    memset(&msg, 0, sizeof(msg));
}

proto_parser::~proto_parser() {
    if (bufhdr)
	mem_free(bufhdr, MSGPKGLEN(bufhdr));
}


int proto_parser::_reset_recv() {
    if (bufhdr)
	mem_free(bufhdr, MSGPKGLEN(bufhdr));
    bufhdr = NULL;
    memset(&msg, 0, sizeof(msg));
    recv_hdr_index = recv_data_index = 0;
    return 0;
}

int proto_parser::_reset_send() {
    send_hdr_index = send_data_index = 0;
    return 0;
}


int proto_parser::__drop(Conn *conn, uint32_t size) {
    int ret = 0;
    uint32_t idx = 0, s = 0;
#define buflen 4096
    char dropbuf[buflen] = {};

    while (recv_data_index < size) {
	idx = recv_data_index % buflen;
	if ((s = buflen - idx) > size - recv_data_index)
	    s = size - recv_data_index;
	if ((ret = conn->Read(dropbuf + idx, s)) < 0)
	    return -1;
	recv_data_index += ret;
    }
    // reaching here. all data has been dropped. we release
    // cur msg and be ready for recv next massage
    dropped_cnt++;
    errno = EAGAIN;
    _reset_recv();
    return -1;

#undef buflen
}    

    
int proto_parser::__recv_hdr(Conn *conn, struct spiohdr *hdr) {
    int64_t nbytes = 0;
    uint32_t size = RHDRLEN;

    while (recv_hdr_index < size) {
	nbytes = conn->Read((char *)hdr + recv_hdr_index, size - recv_hdr_index);
	if (nbytes < 0)
	    return -1;
	recv_hdr_index += nbytes;
    }
    if (package_validate(hdr, NULL) != 0) {
	errno = SPIO_ECHECKSUM;
	return -1;
    }
    return 0;
}
    
int proto_parser::__recv_data(Conn *conn, int c, struct slice *s) {
    int64_t nbytes = 0;
    char *buffer = NULL;
    uint32_t cap = 0, size = 0;

    slice_bufcap(c, s, cap);
    while (recv_data_index < cap) {
	locate_slice_buffer(recv_data_index, s, buffer, size);
	if ((nbytes = conn->Read(buffer, size)) < 0)
	    return -1;
	recv_data_index += nbytes;
    }
    return 0;
}

int proto_parser::__send_hdr(Conn *conn, const struct spiohdr *hdr) {
    int64_t nbytes = 0;
    uint32_t size = RHDRLEN;
    struct spiohdr chdr = *hdr;

    package_makechecksum(&chdr, NULL, 0);
    while (send_hdr_index < size) {
	nbytes = conn->Write((char *)&chdr + send_hdr_index, size - send_hdr_index);
	if (nbytes < 0)
	    return -1;
	send_hdr_index += nbytes;
    }
    return 0;
}

int proto_parser::__send_data(Conn *conn, int c, struct slice *s) {
    int64_t nbytes = 0;
    char *buffer = NULL;
    uint32_t cap = 0, size = 0;

    slice_bufcap(c, s, cap);
    while (send_data_index < cap) {
	locate_slice_buffer(send_data_index, s, buffer, size);
	if ((nbytes = conn->Write(buffer, size)) < 0)
	    return -1;
	send_data_index += nbytes;
    }
    return 0;
}

    
int proto_parser::_recv_massage(Conn *conn, struct nspiomsg **header, int reserve) {
    int ret = 0;
    struct slice s = {};

    if (!header || !conn) {
	errno = EINVAL;
	return -1;
    }

    if ((ret = __recv_hdr(conn, &msg.hdr)) < 0)
	return -1;
    // checking exceed msg_max_size
    if (msg_max_size && msg.hdr.size >= msg_max_size)
	return __drop(conn, msg.hdr.size);

    // Once we have finish recv the whole request header. we allocate a
    // memory arena for storage request data.
    // importance! reserve some space for storage current rt status.
    if (!bufhdr) {
	if (!(bufhdr = (struct nspiomsg *)mem_zalloc(MSGPKGLEN(&msg) + reserve))) {
	    errno = EAGAIN;
	    return -1;
	}
	if (msg.hdr.size)
	    msg.data = (char *)bufhdr + MSGHDRLEN;
	*bufhdr = msg;
    }

    // Second. read data from socket.
    s.len = bufhdr->hdr.size;
    s.data = bufhdr->data;
    if ((ret = __recv_data(conn, 1, &s)) < 0)
	return -1;
    *header = bufhdr;
    bufhdr = NULL;

    _reset_recv();
    return 0;
}

int proto_parser::_send_massage(Conn *conn, struct nspiomsg *header) {
    int ret = 0;
    
    do {
	ret = _send_massage_async(conn, header);
    } while (ret < 0 && errno == EAGAIN);

    return ret;
}

int proto_parser::_send_massage_async(Conn *conn, struct nspiomsg *header) {
    int ret = 0;
    struct slice s = {};
    
    if (!conn || !header) {
	errno = EINVAL;
	return -1;
    }
    if ((ret = __send_hdr(conn, &header->hdr)) < 0)
	return -1;
    s.len = header->hdr.size;
    s.data = header->data;
    if ((ret = __send_data(conn, 1, &s)) < 0)
	return -1;

    _reset_send();
    return ret;
}

int proto_parser::_recv_massage(Conn *conn, struct spiohdr *hdr, struct slice *s) {
    int ret = 0;
    struct slice _s = {};

    if (!conn || !hdr || !s) {
	errno = EINVAL;
	return -1;
    }

    if ((ret = __recv_hdr(conn, &msg.hdr)) < 0)
	return -1;
    if (msg.hdr.size && !msg.data) {
	if (!(msg.data = (char *)mem_zalloc(msg.hdr.size))) {
	    errno = EAGAIN;
	    return -1;
	}
    }
    _s.len = msg.hdr.size;
    _s.data = msg.data;
    if (msg.hdr.size && (ret = __recv_data(conn, 1, &_s)) < 0)
	return -1;
    *s = _s;
    memcpy(hdr, &msg.hdr, RHDRLEN);
    
    _reset_recv();
    return ret;
}

int proto_parser::_send_massage(Conn *conn, const struct spiohdr *hdr, int c, struct slice *s) {
    int ret = 0;
    
    do {
	ret = _send_massage_async(conn, hdr, c, s);
    } while (ret < 0 && errno == EAGAIN);

    return ret;
}
    
int proto_parser::_send_massage_async(Conn *conn, const struct spiohdr *hdr,
				      int c, struct slice *s) {
    int ret = 0;
    
    if (!conn || !s || c <= 0) {
	errno = EINVAL;
	return -1;
    }
    if ((ret = __send_hdr(conn, hdr)) < 0)
	return -1;
    if ((ret = __send_data(conn, c, s)) < 0)
	return -1;

    _reset_send();
    return ret;
}



    

}
