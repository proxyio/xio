from xio import *

svr_eid = sp_endpoint(SP_REQREP, SP_REP);
cli_eid = sp_endpoint(SP_REQREP, SP_REQ);
host = "inproc://py_reqrep";

for i in range(1, 10) :
    sockaddr = host + str(i);
    rc = sp_listen(svr_eid, sockaddr);
    assert (rc == 0);

for i in range(1, 10) :
    sockaddr = host + str(i);
    rc = sp_connect(cli_eid, sockaddr);
    assert (rc == 0);

for i in range(1, 10) :
    msg = Message("Hello world");
    rc = sp_send(cli_eid, msg);
    assert (rc == 0);

    rc, msg1 = sp_recv(svr_eid)
    assert (rc == 0);
    rc = sp_send(svr_eid, msg1);
    assert (rc == 0);

    rc, msg2 = sp_recv(cli_eid)
    assert (rc == 0);
    print "PASS " + str(i);

sp_close(svr_eid);
sp_close(cli_eid);

