package xio

import (
	"strconv"
	"testing"
)

func TestREQREP(t *testing.T) {
	recver := sp_endpoint(SP_REQREP, SP_REP)
	sender := sp_endpoint(SP_REQREP, SP_REQ)
	host := "inproc://go_reqrep"

	for i := 1; i <= 10; i++ {
		sockaddr := host + strconv.Itoa(i)
		if rc := sp_listen(recver, sockaddr); rc != 0 {
			t.Fatal()
		}
	}
	for i := 1; i <= 10; i++ {
		sockaddr := host + strconv.Itoa(i)
		if rc := sp_connect(sender, sockaddr); rc != 0 {
			t.Fatal()
		}
	}
	for i := 1; i <= 10; i++ {
		var msg Msg
		msg.data = []byte ("Hello world");
		if rc := sp_send(sender, msg); rc != 0 {
			t.Fatal()
		}

		if rc, msg := sp_recv(recver); rc != 0 {
			t.Fatal()
		} else if rc = sp_send(recver, msg); rc != 0 {
			t.Fatal()
		}

		if rc, msg := sp_recv(sender); rc != 0 {
			t.Fatal()
		} else {
			println (string(msg.data))
		}
	}
	sp_close(recver)
	sp_close(sender)
}
