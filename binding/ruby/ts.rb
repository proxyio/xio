require "xio"

def assert (boolean)
  if (!boolean)
    puts "error"
    exit
  end
end

recver = sp_endpoint(SP_REQREP, SP_REP)
sender = sp_endpoint(SP_REQREP, SP_REQ)
host = "inproc://py_reqrep"

for i in 1..10
  rc = sp_listen(recver, host + i.to_s)
  assert (rc == 0)
end
  
for i in 1..10
  rc = sp_connect(sender, host + i.to_s)
  assert (rc == 0)
end

for i in 1..10
  rc = sp_send(sender, "Hello world")
  assert (rc == 0)

  req = sp_recv(recver)
  resp = "Hello you ?"
  resp.instance_variable_set("@hdr", req.instance_variable_get("@hdr"))
  rc = sp_send(recver, resp)
  assert (rc == 0)

  resp = sp_recv(sender)
  puts "PASS " + resp + i.to_s
end

sp_close(recver)
sp_close(sender)
