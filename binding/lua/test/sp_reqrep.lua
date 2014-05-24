svr_eid = sp_endpoint(SP_REQREP, SP_REP);
cli_eid = sp_endpoint(SP_REQREP, SP_REQ);
host = "inproc://lua_reqrep";

for i = 1, 10 do
   sockaddr = string.format("%s %d", host, i);
   rc = sp_listen(svr_eid, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   sockaddr = string.format("%s %d", host, i);
   rc = sp_connect(cli_eid, sockaddr);
   assert (rc == 0);
end

for i = 1, 10 do
   msg = xallocubuf("hello word!");
   rc = sp_send(cli_eid, msg);
   assert (rc == 0);

   rc, msg = sp_recv(svr_eid)
   assert (rc == 0);
   rc = sp_send(svr_eid, msg);
   assert (rc == 0);

   rc, msg = sp_recv(cli_eid)
   assert (rc == 0);
   xfreeubuf(msg);
end

sp_close(svr_eid);
sp_close(cli_eid);