import time
from xio import *

recver = sp_endpoint(SP_REQREP, SP_REP);
host = "ipc://consistent_hash_benchmark_sock2";
tcphost = "tcp://127.0.0.1:1882";

assert (sp_listen(recver, host) >= 0);

start_time = time.time ();
recv_counter = 0;    

while (1):
    rc, req = sp_recv(recver);
    if rc:
        continue;
    sp_send(recver, req);
    recv_counter += 1;
    cost = time.time () - start_time;
    if (cost >= 1) :
        start_time = time.time ();
        print "QPS " + str(recv_counter/cost);
        recv_counter = 0;
    
sp_close(recver);
