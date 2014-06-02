<?php

$svr_eid = sp_endpoint($SP_REQREP, $SP_REP);
$cli_eid = sp_endpoint($SP_REQREP, $SP_REQ);
$host = "inproc://py_reqrep";

for ($i = 0; $i < 10; $i++) {
    $sockaddr = $host + strval($i);
    $rc = sp_listen($svr_eid, $sockaddr);
    assert ($rc == 0);
}

for ($i = 0; $i < 10; $i++) {
    $sockaddr = $host + strval($i);
    $rc = sp_connect($cli_eid, $sockaddr);
    assert ($rc == 0);
}

for ($i = 0; $i < 10; $i++) {
    $msg = xallocubuf("Hello php xio");
    $rc = sp_send($cli_eid, $msg);
    assert ($rc == 0);

    $msg1 = sp_recv($svr_eid);
    assert ($msg1 > 0);
    $rc = sp_send($svr_eid, $msg1);
    assert ($rc == 0);

    $msg2 = sp_recv($cli_eid);
    assert ($msg2 > 0);
    xfreeubuf($msg2);
}

sp_close($svr_eid);
sp_close($cli_eid);

?>
