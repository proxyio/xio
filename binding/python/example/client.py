import time
from xio import *

sender = sp_endpoint(SP_REQREP, SP_REQ);
host = "tcp://127.0.0.1:1510";

for i in range(1, 1000) :
    assert (sp_connect(sender, host) == 0);

assert (sp_connect(sender, host) == 0);
    
for i in range(1, 100000) :
    req = Msg ();
    req.data = "Hello world"
    assert (sp_send(sender, req) == 0);

    rc, resp = sp_recv(sender)
    assert (rc == 0);

sp_close(sender);

