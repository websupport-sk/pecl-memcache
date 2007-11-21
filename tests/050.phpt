--TEST--
failure callback throws exception
--SKIPIF--
<?php include 'connect.inc'; if (version_compare(phpversion(), '5.0.0', '<')) die('skip requires PHP >= 5.0.0'); ?>
--FILE--
<?php

include 'connect.inc';

function _callback_server_failure($host, $tcp_port, $udp_port, $error, $errnum) {
	var_dump($host);
	throw new Exception('Test2');
}

$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort, false, 1, 1, 15, true, '_callback_server_failure');
$result = 'Test1';

try {
	$result = @$memcache->set('test_key', 'test-032-01');
}
catch (Exception $e) {
	var_dump($e->getMessage());
}

var_dump($result);

?>
--EXPECTF--
string(%d) "%s"
string(5) "Test2"
string(5) "Test1"
