package xio

/*
 #cgo CFLAGS: -I/usr/local/include -I/usr/include
 #cgo LDFLAGS: -L/usr/local/lib -L/usr/lib -L/usr/local/lib64 -L/usr/lib64 -lxio -luuid

 #include <xio/socket.h>

*/
import "C"

const (
	XPF_TCP      = C.XPF_TCP
	XPF_IPC      = C.XPF_IPC
	XPF_INPROC   = C.XPF_INPROC
	XLISTENER    = C.XLISTENER
	XCONNECTOR   = C.XCONNECTOR
	XSOCKADDRLEN = C.XSOCKADDRLEN
	XL_SOCKET    = C.XL_SOCKET
	XNOBLOCK     = C.XNOBLOCK
	XSNDWIN      = C.XSNDWIN
	XRCVWIN      = C.XRCVWIN
	XSNDBUF      = C.XSNDBUF
	XRCVBUF      = C.XRCVBUF
	XLINGER      = C.XLINGER
	XSNDTIMEO    = C.XSNDTIMEO
	XRCVTIMEO    = C.XRCVTIMEO
	XRECONNECT   = C.XRECONNECT
	XSOCKTYPE    = C.XSOCKTYPE
	XSOCKPROTO   = C.XSOCKPROTO
	XTRACEDEBUG  = C.XTRACEDEBUG
)

func xallocubuf(size int) *C.char {
	var ubuf *C.char
	ubuf = C.xallocubuf(C.int(size))
	return ubuf
}

func xfreeubuf(ubuf *C.char) {
	C.xfreeubuf(ubuf)
}

func xubuflen(ubuf *C.char) int {
	return int(C.xubuflen(ubuf))
}

func xlisten(sockaddr string) int {
	return int(C.xlisten(C.CString(sockaddr)))
}

func xaccept(fd int) int {
	return int(C.xaccept(C.int(fd)))
}

func xconnect(sockaddr string) int {
	return int(C.xconnect(C.CString(sockaddr)))
}

func xrecv(fd int) (rc int, ubuf *C.char) {
	rc = int(C.xrecv(C.int(fd), &ubuf))
	return rc, ubuf
}

func xsend(fd int, ubuf *C.char) (rc int) {
	rc = int(C.xsend(C.int(fd), ubuf))
	return rc
}

func xclose(fd int) (rc int) {
	rc = int(C.xclose(C.int(fd)))
	return rc
}

func xsetopt(fd int, level int, opt int, optval *C.void, optlen int) (rc int) {
	return rc
}

func xgetopt(fd int, level int, opt int, optval *C.void, optlen *int) (rc int) {
	return rc
}
