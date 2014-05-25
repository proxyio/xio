package xio

import (
	"strconv"
	"testing"
)

func TestREQREP(t *testing.T) {
	svr_eid := sp_endpoint(SP_REQREP, SP_REP)
	cli_eid := sp_endpoint(SP_REQREP, SP_REQ)
	host := "inproc://go_reqrep"

	for i := 1; i <= 10; i++ {
		sockaddr := host + strconv.Itoa(i)
		if rc := sp_listen(svr_eid, sockaddr); rc != 0 {
			t.Fatal()
		}
	}
	for i := 1; i <= 10; i++ {
		sockaddr := host + strconv.Itoa(i)
		if rc := sp_connect(cli_eid, sockaddr); rc != 0 {
			t.Fatal()
		}
	}
	for i := 1; i <= 10; i++ {
		msg := xallocubuf(12)
		if rc := sp_send(cli_eid, msg); rc != 0 {
			t.Fatal()
		}

		if rc, msg := sp_recv(svr_eid); rc != 0 {
			t.Fatal()
		} else if rc = sp_send(svr_eid, msg); rc != 0 {
			t.Fatal()
		}

		if rc, msg := sp_recv(cli_eid); rc != 0 {
			t.Fatal()
		} else {
			xfreeubuf(msg)
		}
	}
	sp_close(svr_eid)
	sp_close(cli_eid)
}
