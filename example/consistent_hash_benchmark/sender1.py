import time
import random
import string
from xio import *

buf = "";
for i in range (1, 1000) :
    buf += string.join(random.sample('1234567890qwertyuiopasdfghjkl;,./!@#$%^&*())_+~', 10));

sender = sp_endpoint(SP_REQREP, SP_REQ);
host = "ipc://consistent_hash_benchmark_sock";
tcphost = "tcp://127.0.0.1:188";

for i in range(1, 4) :
    sp_connect(sender, host + str(i));
    print "connect to " + host + str(i);

start_time = time.time ();
send_counter = 0;    

while (1):
    req = Msg ();
    req.data = buf;
    assert (sp_send(sender, req) == 0);

    rc, resp = sp_recv(sender)
    assert (rc == 0);
    send_counter = send_counter + 1;
    cost = time.time () - start_time;
    if (cost >= 1):
        start_time = time.time ();
        print "QPS " + str(send_counter/cost);
        send_counter = 0;

sp_close(recver);
