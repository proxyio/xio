package xio

/*
 #cgo CFLAGS: -I/usr/local/include -I/usr/include
 #cgo LDFLAGS: -L/usr/local/lib -L/usr/lib -L/usr/local/lib64 -L/usr/lib64 -lxio -luuid

 #include <string.h>
 #include <xio/sp.h>
 #include <xio/sp_reqrep.h>
 
*/
import "C"
import "unsafe"


const (
	SP_REQREP = C.SP_REQREP
	SP_BUS    = C.SP_BUS
	SP_PUBSUB = C.SP_PUBSUB
	SP_REQ    = C.SP_REQ
	SP_REP    = C.SP_REP
	SP_PROXY  = C.SP_PROXY
)

type Msg struct {
	data   []byte
	hdr    [][]byte
}

func sp_endpoint (sp_family int, sp_type int) (eid int) {
	eid = int (C.sp_endpoint (C.int (sp_family), C.int (sp_type)))
	return eid
}

func sp_close (eid int) (rc int) {
	rc = int (C.sp_close (C.int (eid)))
	return rc
}

func sp_recv(eid int) (rc int, msg Msg) {
	var (
		ubuf *C.char
		cur  *C.char
		leng int
	)

	if rc = int (C.sp_recv (C.int (eid), &ubuf)); rc == 0 {
		msg.data = C.GoBytes (unsafe.Pointer (ubuf), C.ubuf_len (ubuf))
		cur = C.ubufctl_first (ubuf)
		msg.hdr = append (msg.hdr, C.GoBytes (unsafe.Pointer (cur), C.ubuf_len (cur)))
		for leng = int (C.ubufctl_num (ubuf)); leng > 0; leng-- {
			cur = C.ubufctl_next (ubuf, cur)
			msg.hdr = append (msg.hdr, C.GoBytes (unsafe.Pointer (cur), C.ubuf_len (cur)))
		}
		C.ubuf_free (ubuf)
	}
	return rc, msg
}

func sp_send(eid int, msg Msg) (rc int) {
	var (
		ubuf *C.char
		cur  *C.char
	)

	ubuf = C.ubuf_alloc (C.int(len(msg.data)))
	C.memcpy (unsafe.Pointer(ubuf), unsafe.Pointer (&msg.data[0]), C.size_t (len (msg.data)))

	for _, s := range msg.hdr {
		cur = C.ubuf_alloc (C.int(len (s)))
		C.memcpy (unsafe.Pointer(cur), unsafe.Pointer (&s[0]), C.size_t (len (s)))
		C.ubufctl_add (ubuf, cur)
	}
	if rc = int (C.sp_send (C.int (eid), ubuf)); rc != 0 {
		C.ubuf_free (ubuf)
	}
	return rc
}


func sp_add(eid int, sockfd int) (rc int) {
	rc = int (C.sp_add (C.int (eid), C.int (sockfd)))
	return rc
}

func sp_rm(eid int, sockfd int) (rc int) {
	rc = int (C.sp_rm (C.int (eid), C.int (sockfd)))
	return rc
}

func sp_connect(eid int, sockaddr string) (rc int) {
	rc = int (C.sp_connect(C.int (eid), C.CString(sockaddr)))
	return rc
}

func sp_listen(eid int, sockaddr string) (rc int) {
	rc = int (C.sp_listen (C.int (eid), C.CString (sockaddr)))
	return rc
}
