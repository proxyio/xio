from xio import *

recver = sp_endpoint(SP_REQREP, SP_REP);
host = "tcp://127.0.0.1:1510";

assert (sp_listen(recver, host) == 0);

for i in range(1, 1000000) :
    rc, req = sp_recv(recver)
    assert (rc == 0);
    resp = Msg ();
    resp.data = "Hello you ?";
    resp.hdr = req.hdr;
    assert (sp_send(recver, resp) == 0);

sp_close(recver);
