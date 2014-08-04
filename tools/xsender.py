#!/bin/env python

import time
import sys
import random
import string
from xio import *

if (len (sys.argv) != 4) :
    print "Usage: xsender sockaddr size count";
    exit(0);

host = sys.argv[1];
size = int (sys.argv[2]);
count = int (sys.argv[3]);

buf = "hello xio";
for i in range (1, size) :
    buf += string.join(random.sample('1234567890qwertyuiopasdfghjkl;,./!@#$%^&*())_+~', 1));

sender = sp_endpoint(SP_REQREP, SP_REQ);
for i in range (0, 20) :
    assert (sp_connect(sender, host) >= 0);

start_time = time.time ();
thr = 0;    

while (count > 0):
    for i in range (0, 200) :
        req = Msg ();
        req.data = buf;
        while (sp_send(sender, req) != 0) :
            time.sleep (0.001);

    for i in range (0, 200) :
        rc, resp = sp_recv(sender)
        assert (rc == 0);
        thr = thr + 1;
        count -= 1;

    cost = time.time () - start_time;
    if (cost >= 1):
        start_time = time.time ();
        print "qps " + str(thr/cost);
        thr = 0;

sp_close(sender);
