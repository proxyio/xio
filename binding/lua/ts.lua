require("xio")

recver = sp_endpoint (SP_REQREP, SP_REP);
sender = sp_endpoint (SP_REQREP, SP_REQ);
host = "inproc://lua_reqrep";

for i = 1, 10 do
   sockaddr = string.format ("%s %d", host, i);
   rc = sp_listen (recver, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   sockaddr = string.format ("%s %d", host, i);
   rc = sp_connect (sender, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   rc = sp_send (sender, {data = "Hello world"});
   assert (rc == 0);

   rc, req = sp_recv (recver)
   assert (rc == 0);
   rc = sp_send (recver, {data = "Hello you ?", hdr = req.hdr});
   assert (rc == 0);

   rc = sp_send (recver, {data = "Hello you ?", hdr = req.hdr});
   assert (rc == 0);
   
   rc, resp = sp_recv (sender);
   assert (rc == 0);
   rc, resp = sp_recv (sender);
   assert (rc == 0);

   print ("PASS ", resp.data, i);
end

sp_close (recver);
sp_close (sender);