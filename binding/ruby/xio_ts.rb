require "xio"

svr_eid = sp_endpoint(SP_REQREP, SP_REP)
cli_eid = sp_endpoint(SP_REQREP, SP_REQ)
host = "inproc://py_reqrep"

for i in 1..10
  sockaddr = host + i.to_s
  rc = sp_listen(svr_eid, sockaddr)
  if (rc != 0)
    puts "bug"
    return
  end
end
  
for i in 1..10
  sockaddr = host + i.to_s
  rc = sp_connect(cli_eid, sockaddr)
  if (rc != 0)
    puts "bug"
    return
  end
end

for i in 1..10
  msg = xallocubuf("Hello ruby xio")
  rc = sp_send(cli_eid, msg)
  if (rc != 0)
    puts "bug"
    return
  end

  msg1 = sp_recv(svr_eid)
  if (msg1 <= 0)
    puts "bug"
    return
  end

  rc = sp_send(svr_eid, msg1)
  if (rc != 0)
    puts "bug"
    return
  end

  msg2 = sp_recv(cli_eid)
  if (msg2 <= 0)
    puts "bug"
    return
  end
  xfreeubuf(msg2)
  puts "PASS " + i.to_s
end

sp_close(svr_eid)
sp_close(cli_eid)
