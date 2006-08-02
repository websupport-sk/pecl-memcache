--TEST--
memcache->addServer() with server failure callback
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

function _callback_server_failure($host, $port) {
	var_dump($host);
	var_dump($port);

	global $memcache;
	$memcache->setServerParams($host, $port, 1, -1, false, '_callback_server_failure');
}

// Test function callback using addServer()
$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort, false, 1, 1, 15, true, '_callback_server_failure');

$result1 = $memcache->set('test_key', 'test-032-01');
var_dump($result1);

class MemcachedFailureHandler {
	function _callback_server_failure($host, $port) {
		var_dump($host);
		var_dump($port);
	}
}

// Test OO callback using setServerParams()
$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort, false);
$result2 = $memcache->setServerParams($nonExistingHost, $nonExistingPort, 1, 15, true, 
	array(new MemcachedFailureHandler(), '_callback_server_failure'));
var_dump($result2);

$result3 = $memcache->set('test_key', 'test-032-01');
var_dump($result3);

// Test giving non-existing callback to addServer()
$memcache = new Memcache();
$result4 = @$memcache->addServer($nonExistingHost, $nonExistingPort, false, 1, 1, 15, true, 'non_existing_user_function');
var_dump($result4);

// Test giving non-existing callback to setServerParams()
$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort, false);
$result5 = @$memcache->setServerParams($nonExistingHost, $nonExistingPort, 1, 15, true, 'non_existing_user_function');
var_dump($result5);

// Test resetting callback to null
$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort, false, 1, 1, 15, true, '_callback_server_failure');
$result6 = $memcache->setServerParams($nonExistingHost, $nonExistingPort, 1, 15, true, null);
$result7 = $memcache->set('test_key', 'test-032-01');

var_dump($result6);
var_dump($result7);

?>
--EXPECTF--
string(%d) "%s"
int(%d)
bool(false)
bool(true)
string(%d) "%s"
int(%d)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
