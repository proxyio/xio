from xio import *

msg = Msg ();
msg.data = "Hello world";
msg.hdr = "Hello you ?";

print msg.hdr;
print msg.data;

recver = sp_endpoint(SP_REQREP, SP_REP);
sender = sp_endpoint(SP_REQREP, SP_REQ);
host = "inproc://py_reqrep";

for i in range(1, 10) :
    assert (sp_listen(recver, host + str(i)) >= 0);

for i in range(1, 10) :
    assert (sp_connect(sender, host + str(i)) >= 0);

for i in range(1, 10) :
    req = Msg ();
    req.data = "Hello world"
    assert (sp_send(sender, req) == 0);

    rc, req = sp_recv(recver)
    assert (rc == 0);
    resp = Msg ();
    resp.data = "Hello you ?";
    resp.hdr = req.hdr;
    assert (sp_send(recver, resp) == 0);

    rc, resp = sp_recv(sender)
    assert (rc == 0);
    print "PASS " + resp.data + str(i);

sp_close(recver);
sp_close(sender);

