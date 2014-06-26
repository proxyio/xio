from xio import *

recver = sp_endpoint(SP_REQREP, SP_REP);
sender = sp_endpoint(SP_REQREP, SP_REQ);
host = "inproc://py_reqrep";

for i in range(1, 10) :
    assert (sp_listen(recver, host + str(i)) == 0);

for i in range(1, 10) :
    assert (sp_connect(sender, host + str(i)) == 0);

for i in range(1, 10) :
    assert (sp_send(sender, "Hello world") == 0);

    rc, msg = sp_recv(recver)
    assert (rc == 0);
    assert (sp_send(recver, msg.Response("Hello you ?")) == 0);

    rc, msg = sp_recv(sender)
    assert (rc == 0);
    print "PASS " + str(msg) + str(i);

sp_close(recver);
sp_close(sender);

