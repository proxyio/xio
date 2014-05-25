package xio

/*
 #cgo CFLAGS: -I/usr/local/include -I/usr/include
 #cgo LDFLAGS: -L/usr/local/lib -L/usr/lib -L/usr/local/lib64 -L/usr/lib64 -lxio -luuid

 #include <xio/sp.h>
 #include <xio/sp_reqrep.h>

*/
import "C"

const (
	SP_REQREP = C.SP_REQREP
	SP_BUS    = C.SP_BUS
	SP_PUBSUB = C.SP_PUBSUB
	SP_REQ    = C.SP_REQ
	SP_REP    = C.SP_REP
	SP_PROXY  = C.SP_PROXY
)

func sp_endpoint(sp_family int, sp_type int) (eid int) {
	eid = int(C.sp_endpoint(C.int(sp_family), C.int(sp_type)))
	return eid
}

func sp_close(eid int) (rc int) {
	rc = int(C.sp_close(C.int(eid)))
	return rc
}

func sp_recv(eid int) (rc int, ubuf *C.char) {
	rc = int(C.sp_recv(C.int(eid), &ubuf))
	return rc, ubuf
}

func sp_send(eid int, ubuf *C.char) (rc int) {
	rc = int(C.sp_send(C.int(eid), ubuf))
	return rc
}

func sp_add(eid int, sockfd int) (rc int) {
	rc = int(C.sp_add(C.int(eid), C.int(sockfd)))
	return rc
}

func sp_rm(eid int, sockfd int) (rc int) {
	rc = int(C.sp_rm(C.int(eid), C.int(sockfd)))
	return rc
}

func sp_connect(eid int, sockaddr string) (rc int) {
	rc = int(C.sp_connect(C.int(eid), C.CString(sockaddr)))
	return rc
}

func sp_listen(eid int, sockaddr string) (rc int) {
	rc = int(C.sp_listen(C.int(eid), C.CString(sockaddr)))
	return rc
}
