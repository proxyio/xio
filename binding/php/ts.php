<?php

if(!extension_loaded('xio')) {
	dl('xio.' . PHP_SHLIB_SUFFIX);
}
if(!extension_loaded('xio')) {
	die("xio extension is not avaliable, please compile it.\n");
}

$recver = sp_endpoint(SP_REQREP, SP_REP);
$sender = sp_endpoint(SP_REQREP, SP_REQ);
$host = "inproc://py_reqrep";

for ($i = 0; $i < 10; $i++) {
	$sockaddr = $host . strval($i);
	$rc = sp_listen($recver, $sockaddr);
	assert ($rc == 0);
}

for ($i = 0; $i < 10; $i++) {
	$sockaddr = $host . strval($i);
	$rc = sp_connect($sender, $sockaddr);
	assert ($rc == 0);
}

for ($i = 0; $i < 10; $i++) {
	$req = new Msg();
	$req->data = "Hello world";
	assert ($rc = sp_send($sender, $req) == 0);

	$req = new Msg();
	assert (($rc = sp_recv($recver, $req)) == 0);

	/* why coredump here */
	$tmp = $req->hdr;

	$req->data = "Hello you ?";
	assert (($rc = sp_send($recver, $req)) == 0);
	assert (($rc = sp_send($recver, $req)) == 0);

	$resp = new Msg();
	assert (($rc = sp_recv($sender, $resp)) == 0);
	assert (($rc = sp_recv($sender, $resp)) == 0);

	print("PASS " . $req->data . "\n");
}

sp_close($recver);
sp_close($sender);

?>
