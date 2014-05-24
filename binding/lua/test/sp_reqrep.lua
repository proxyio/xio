local str = "hello proxyio";
local msg = xallocubuf(str);
local length = xubuflen(msg);
assert(length == string.len(str))
xfreeubuf(msg);
print(SP_REQREP);