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
	src/socket/xlisten.c \
	src/socket/mul/mul_listener.c \
	src/socket/stats.c \
	src/socket/xsend.c \
	src/socket/xrecv.c \
	src/socket/eventnotify.c \
	src/socket/worker.c \
	src/socket/xbind.c \
	src/socket/global.c \
	src/socket/xclose.c \
	src/socket/xconnect.c \
	src/socket/tcp/tcpipc_listener.c \
	src/socket/tcp/tcpipc.c \
	src/socket/sock.c \
	src/socket/xgetsockopt.c \
	src/socket/skbuf.c \
	src/socket/inproc/inproc.c \
	src/socket/inproc/inproc_listener.c


XIO_POLL = \
	src/poll/poll_struct.c \
	src/poll/pglobal.c \
	src/poll/stats.c \
	src/poll/xselect.c \
	src/poll/xeventpoll.c

XIO_SP = \
	src/sp/sp_module.c \
	src/sp/sp_getopt.c \
	src/sp/sp_close.c \
	src/sp/sp_add.c \
	src/sp/sp_endpoint.c \
	src/sp/sp_send.c \
	src/sp/reqrep/pipeline.c \
	src/sp/reqrep/rep_ep.c \
	src/sp/reqrep/req_ep.c \
	src/sp/reqrep/rr_algo_rrbin.c \
	src/sp/reqrep/rr_algo_weight_rrbin.c \
	src/sp/pubsub/pub.c \
	src/sp/pubsub/sub.c \
	src/sp/bus/bus.c \
	src/sp/sp_rm.c \
	src/sp/sp_recv.c \
	src/sp/sp_setopt.c

XIO_UTILS = \
	src/utils/transport.c \
	src/utils/ipc/ipc_getopt.c \
	src/utils/ipc/ipc_setopt.c \
	src/utils/ipc/ipc.c \
	src/utils/timer.c \
	src/utils/condition.c \
	src/utils/taskpool.c \
	src/utils/mutex.c \
	src/utils/eventloop.c \
	src/utils/filepath.c \
	src/utils/krb.c \
	src/utils/alloc.c \
	src/utils/spinlock.c \
	src/utils/krb_augmented.c \
	src/utils/efd.c \
	src/utils/waitgroup.c \
	src/utils/thread.c \
	src/utils/atomic.c \
	src/utils/module.c \
	src/utils/tcp/tcp.c \
	src/utils/tcp/tcp_getopt.c \
	src/utils/tcp/tcp_setopt.c \
	src/utils/bufio.c \
	src/utils/mstats_base.c \
	src/utils/slab.c \
	src/utils/crc16.c \
	src/utils/sockaddr.c

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
	tests/stl_ts \
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
	tests/sp_bus_ts \
	tests/sp_reqrep_ts \
	tests/sp_pubsub_ts \
	tests/sp_pipeline_ts

check_PROGRAMS = \
	$(UTILS_TESTS)

TESTS = $(check_PROGRAMS)

#  Building lua binding
if LUA
xinclude_HEADERS += binding/lua/xio_if.h
LUA_SOURCES = \
	binding/lua/xio_if.c \
	binding/lua/xio_if.h

libxio_la_SOURCES += $(LUA_SOURCES)
endif
