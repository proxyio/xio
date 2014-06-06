################################################################################
#  xio library                                                                 #
################################################################################

EXTRA_DIST =


INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/src/


xincludedir = $(includedir)/xio
xinclude_HEADERS = \
	include/xio/poll.h \
	include/xio/cplusplus_endif.h \
	include/xio/sp_reqrep.h \
	include/xio/sp_pubsub.h \
	include/xio/cmsghdr.h \
	include/xio/cplusplus_define.h \
	include/xio/sp.h \
	include/xio/socket.h

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
	src/socket/xgetsockopt.c \
	src/socket/xinproc.h \
	src/socket/xmessage.c \
	src/socket/xmessage.h \
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
	src/sp/reqrep/pipeline.c \
	src/sp/reqrep/rep_ep.c \
	src/sp/reqrep/rep_ep.h \
	src/sp/reqrep/req_ep.c \
	src/sp/reqrep/req_ep.h \
	src/sp/pubsub/pub.h \
	src/sp/pubsub/pub.c \
	src/sp/pubsub/sub.h \
	src/sp/pubsub/sub.c \
	src/sp/sp_rm.c \
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

LDADD = libxio.la


#  tools
bin_PROGRAMS =

#xiobc_SOURCES = \
#	tools/xiobc.c

#bin_PROGRAMS += xiobc

#  automated tests
UTILS_TESTS = \
	tests/bufio_ts \
	tests/krb_ts \
	tests/modstat_ts \
	tests/path_ts \
	tests/runner_ts \
	tests/sync_ts \
	tests/socket_ts \
	tests/xubuf_ts \
	tests/xsocket_ts \
	tests/poll_ts \
	tests/sp_reqrep_ts \
	tests/sp_pipeline_ts

check_PROGRAMS = \
	$(UTILS_TESTS)

TESTS = $(check_PROGRAMS)


#  Building python binding
if PYTHON
xinclude_HEADERS += binding/python/xio_if.h
PYTHON_SOURCES = \
	binding/python/xio_if.c \
	binding/python/xio_if.h
libxio_la_SOURCES += $(PYTHON_SOURCES)
endif

#  Building lua binding
if LUA
xinclude_HEADERS += binding/lua/xio_if.h
LUA_SOURCES = \
	binding/lua/xio_if.c \
	binding/lua/xio_if.h

libxio_la_SOURCES += $(LUA_SOURCES)
endif
