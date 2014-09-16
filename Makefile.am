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

XIO_INIT = \
	src/init/__init.c

XIO_MSGBUF = \
	src/msgbuf/msgbuf.c \
	src/msgbuf/msgbuf_head.c

XIO_EV = \
	src/ev/ev_stats.c \
	src/ev/eventpoll.c \
	src/ev/ev.c

XIO_REX = \
	src/rex/rex.c

XIO_SOCKET = \
	src/socket/setsockopt.c \
	src/socket/listen.c \
	src/socket/mix/mix_listener.c \
	src/socket/mix/mix_getsetor.c \
	src/socket/sstats.c \
	src/socket/send.c \
	src/socket/recv.c \
	src/socket/eventnotify.c \
	src/socket/sockaddr.c \
	src/socket/bind.c \
	src/socket/close.c \
	src/socket/connect.c \
	src/socket/sio/sio_uev.c \
	src/socket/sio/sio_listener.c \
	src/socket/sio/sio_getsetor.c \
	src/socket/sio/sio.c \
	src/socket/sockbase.c \
	src/socket/getsockopt.c \
	src/socket/inproc/inproc.c \
	src/socket/inproc/inproc_getsetor.c \
	src/socket/inproc/inproc_listener.c


XIO_POLL = \
	src/poll/poll_struct.c \
	src/poll/stats.c \
	src/poll/select.c \
	src/poll/eventpoll.c

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
	src/sp/reqrep/lbs_rrbin.c \
	src/sp/reqrep/lbs_ulhash.c \
	src/sp/reqrep/lbs_conhash.c \
	src/sp/pubsub/pub.c \
	src/sp/pubsub/sub.c \
	src/sp/bus/bus.c \
	src/sp/sp_rm.c \
	src/sp/sp_recv.c \
	src/sp/sp_setopt.c

XIO_UTILS = \
	src/utils/timer.c \
	src/utils/i64_rb.c \
	src/utils/consistent_hash.c \
	src/utils/md5.c \
	src/utils/str_rb.c \
	src/utils/condition.c \
	src/utils/taskpool.c \
	src/utils/mutex.c \
	src/utils/filepath.c \
	src/utils/krb.c \
	src/utils/alloc.c \
	src/utils/spinlock.c \
	src/utils/krb_augmented.c \
	src/utils/efd.c \
	src/utils/waitgroup.c \
	src/utils/thread.c \
	src/utils/atomic.c \
	src/utils/bufio.c \
	src/utils/mstats_base.c \
	src/utils/mem_cache.c \
	src/utils/str_array.c \
	src/utils/unorder_p_array.c \
	src/utils/crc16.c

libxio_la_SOURCES = \
	$(XIO_INIT) \
	$(XIO_MSGBUF) \
	$(XIO_UTILS) \
	$(XIO_SOCKET) \
	$(XIO_POLL) \
	$(XIO_SP) \
	$(XIO_REX) \
	$(XIO_EV)

LDADD = libxio.la


#  tools
bin_PROGRAMS = \
	perf/lat_sender \
	perf/lat_recver \
	perf/thr_sender \
	perf/thr_recver

#  automated tests
UTILS_TESTS = \
	tests/stl_ts \
	tests/rex_ts \
	tests/ev_ts \
	tests/bufio_ts \
	tests/rb_ts \
	tests/modstat_ts \
	tests/path_ts \
	tests/taskpool_ts \
	tests/sync_ts \
	tests/msgbuf_ts \
	tests/socket_ts \
	tests/poll_ts \
	tests/sp_bus_ts \
	tests/sp_reqrep_ts \
	tests/sp_pubsub_ts \
	tests/sp_pipeline_ts

check_PROGRAMS = \
	$(UTILS_TESTS)

TESTS = $(check_PROGRAMS)

# Building lua binding
if LUA
LUA_SOURCES = \
	binding/lua/lua_xio.c

libxio_la_SOURCES += $(LUA_SOURCES)
endif
