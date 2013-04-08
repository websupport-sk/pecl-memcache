--TEST--
memcache->addServer() adding server in failed mode
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

// Run hash destination test
$memcache = new Memcache();
$memcache->addServer($host, $port, false);
$memcache->addServer($host2, $port2, false);

$memcache1 = memcache_connect($host, $port);
$memcache2 = memcache_connect($host2, $port2);

$memcache1->set($balanceKey1, '', false, 2);
$memcache1->set($balanceKey1, '', false, 2);
$memcache2->set($balanceKey1, '', false, 2);
$memcache2->set($balanceKey2, '', false, 2);

$result1 = $memcache->set($balanceKey1, 'test-030-01', false, 2);	// hashes to $host2
$result2 = $memcache->set($balanceKey2, 'test-030-02', false, 2);	// hashes to $host1

var_dump($result1);
var_dump($result2);

$result4 = $memcache1->get($balanceKey1);	// return false; key1 is at $host2 
$result5 = $memcache1->get($balanceKey2);
$result6 = $memcache2->get($balanceKey1);	
$result7 = $memcache2->get($balanceKey2);	// return false; key2 is at $host1

var_dump($result4);
var_dump($result5);
var_dump($result6);
var_dump($result7);

// Add server in failed mode
$memcache = new Memcache();
$memcache->addServer($host, $port, false);
$memcache->addServer($host2, $port2, false, 1, 1, -1, false);

$result8 = $memcache->set($balanceKey1, 'test-030-03', false, 1);	// hashes to $host2
$result9 = $memcache->set($balanceKey2, 'test-030-04', false, 1);	// hashes to $host1

var_dump($result8);
var_dump($result9);

$result10 = $memcache1->get($balanceKey1);	// key should be $host1 (failed over from $host2)
$result11 = $memcache1->get($balanceKey2);	// key should always be at $host1

var_dump($result10);
var_dump($result11);

// Single server in failed mode
function _callback_server_failure($host, $port) {
	print "_callback_server_failure()\n";
}

$memcache = new Memcache();
$memcache->addServer($host, $port, true, 1, 1, -1, false, '_callback_server_failure');
$result13 = $memcache->add('test_key', 'Test');
$result14 = $memcache->get('test_key');

var_dump($result13);
var_dump($result14);

?>
--EXPECT--
bool(true)
bool(true)
string(0) ""
string(11) "test-030-02"
string(11) "test-030-01"
string(0) ""
bool(true)
bool(true)
string(11) "test-030-03"
string(11) "test-030-04"
bool(false)
bool(false)
