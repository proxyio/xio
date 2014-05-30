################################################################################
#  xio library                                                                 #
################################################################################

CURRENTPATH = ./
INCLUDES = -I$(CURRENTPATH)/include/xio -I$(CURRENTPATH)/src/

lib_LTLIBRARIES = libxio.la

XIO_SOCKET = \
	src/socket/xsetsockopt.c \
	src/socket/xgb.h \
	src/socket/xlisten.c \
	src/socket/mul/xmul_listener.c \
	src/socket/xsend.c \
	src/socket/xsock.h \
	src/socket/xrecv.c \
	src/socket/xeventnotify.c \
	src/socket/xcpu.c \
	src/socket/xbind.c \
	src/socket/xgb.c \
	src/socket/xclose.c \
	src/socket/xconnect.c \
	src/socket/tcp/xtcpipc_listener.c \
	src/socket/tcp/xtcpipc.c \
	src/socket/xsock.c \
	src/socket/xmsg.h \
	src/socket/xgetsockopt.c \
	src/socket/xinproc.h \
	src/socket/xmsg.c \
	src/socket/xtcpipc.h \
	src/socket/inproc/xinproc.c \
	src/socket/inproc/xinproc_listener.c \
	src/socket/xcpu.h

XIO_POLL = \
	src/poll/xeventpoll.h \
	src/poll/xpoll_struct.c \
	src/poll/xselect.c \
	src/poll/xeventpoll.c

XIO_SP = \
	src/sp/sp_module.c \
	src/sp/sp_getopt.c \
	src/sp/sp_close.c \
	src/sp/sp_add.c \
	src/sp/sp_endpoint.c \
	src/sp/bus/bus.h \
	src/sp/sp_send.c \
	src/sp/reqrep/req_ep.h \
	src/sp/reqrep/pipeline.c \
	src/sp/reqrep/rep_ep.c \
	src/sp/reqrep/rep_ep.h \
	src/sp/reqrep/req_ep.c \
	src/sp/sp_rm.c \
	src/sp/sp_hdr.c \
	src/sp/pubsub/sub.h \
	src/sp/pubsub/pub.h \
	src/sp/sp_recv.c \
	src/sp/sp_hdr.h \
	src/sp/sp_setopt.c \
	src/sp/sp_module.h

XIO_UTILS = \
	src/utils/spinlock.h \
	src/utils/transport.c \
	src/utils/ipc/ipc_getopt.c \
	src/utils/ipc/ipc_setopt.c \
	src/utils/ipc/ipc.c \
	src/utils/ipc/ipc.h \
	src/utils/bufio.h \
	src/utils/timer.c \
	src/utils/condition.c \
	src/utils/filepath.h \
	src/utils/skrb_sync.h \
	src/utils/taskpool.c \
	src/utils/mutex.c \
	src/utils/alloc.h \
	src/utils/timer.h \
	src/utils/eventloop.c \
	src/utils/filepath.c \
	src/utils/base.h \
	src/utils/atomic.h \
	src/utils/krb.c \
	src/utils/skrb.h \
	src/utils/map.h \
	src/utils/list.h \
	src/utils/efd.h \
	src/utils/alloc.c \
	src/utils/thread.h \
	src/utils/crc.h \
	src/utils/krb.h \
	src/utils/krb_augmented.h \
	src/utils/condition.h \
	src/utils/spinlock.c \
	src/utils/krb_augmented.c \
	src/utils/efd.c \
	src/utils/modstat.h \
	src/utils/waitgroup.c \
	src/utils/taskpool.h \
	src/utils/thread.c \
	src/utils/atomic.c \
	src/utils/module.c \
	src/utils/tcp/tcp.c \
	src/utils/tcp/tcp.h \
	src/utils/tcp/tcp_getopt.c \
	src/utils/tcp/tcp_setopt.c \
	src/utils/bufio.c \
	src/utils/modstat.c \
	src/utils/slab.h \
	src/utils/slab.c \
	src/utils/crc16.c \
	src/utils/sockaddr.c \
	src/utils/sockaddr.h \
	src/utils/waitgroup.h \
	src/utils/eventloop.h \
	src/utils/transport.h \
	src/utils/mutex.h

libxio_la_SOURCES = \
	$(XIO_UTILS) \
	$(XIO_SOCKET) \
	$(XIO_POLL) \
	$(XIO_SP)


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxio.pc
