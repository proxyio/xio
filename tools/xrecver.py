#!/bin/env python

import sys
import time
from xio import *

if (len (sys.argv) != 2) :
    print "Usage: xrecver sockaddr";
    exit(0);

host = sys.argv[1];
recver = sp_endpoint(SP_REQREP, SP_REP);
assert (sp_listen(recver, host) >= 0);

start_time = time.time ();
thr = 0;    

while (1):
    rc, req = sp_recv(recver);
    if rc:
        continue;
    while (sp_send(recver, req) != 0):
        time.sleep (0.001);
        
    thr += 1;
    cost = time.time () - start_time;
    if (cost >= 1) :
        start_time = time.time ();
        print "qps " + str(thr/cost);
        thr = 0;

sp_close(recver);
