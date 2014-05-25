package xio

/*
 #cgo CFLAGS: -I/usr/local/include -I/usr/include
 #cgo LDFLAGS: -L/usr/local/lib -L/usr/lib -L/usr/local/lib64 -L/usr/lib64 -lxio -luuid

 #include <xio/poll.h>

 */
import "C"


const (
	XPOLLIN      = C.XPOLLIN
	XPOLLOUT     = C.XPOLLOUT
	XPOLLERR     = C.XPOLLERR
	XPOLL_ADD    = C.XPOLL_ADD
	XPOLL_DEL    = C.XPOLL_DEL
	XPOLL_MOD    = C.XPOLL_MOD
)

type PollEnt struct {
	fd int
	self interface{}
	events int
	happened int
}

func xpoll_create() (pollid int) {
	pollid = int(C.xpoll_create())
	return pollid
}

func xpoll_close(pollid int) (rc int) {
	rc = int(C.xpoll_close(C.int(pollid)))
	return rc
}

func xpoll_ctl(pollid int, op int, ent *PollEnt) (rc int) {
	var et C.struct_poll_ent
	et.fd = C.int(ent.fd)
	et.events = C.int(ent.events)
	et.happened = C.int(0)
	rc = int(C.xpoll_ctl(C.int(pollid), C.int(op), &et))
	return rc
}
