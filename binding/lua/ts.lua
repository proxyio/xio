require("xio")

recver = sp_endpoint(SP_REQREP, SP_REP);
sender = sp_endpoint(SP_REQREP, SP_REQ);
host = "inproc://lua_reqrep";

for i = 1, 10 do
   sockaddr = string.format("%s %d", host, i);
   rc = sp_listen(recver, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   sockaddr = string.format("%s %d", host, i);
   rc = sp_connect(sender, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   msg = ubuf_alloc ("hello word!");
   rc = sp_send(sender, msg);
   assert (rc == 0);

   rc, msg = sp_recv(recver)
   assert (rc == 0);
   rc = sp_send(recver, msg);
   assert (rc == 0);

   rc, msg = sp_recv(sender)
   assert (rc == 0);
   ubuf_free (msg);

   print("PASS ", i);
end

sp_close(recver);
sp_close(sender);