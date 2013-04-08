--TEST--
memcache->close(), memcache->get()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$memcache->addServer($host2, $port2);
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result1 = $memcache->close();
var_dump($result1);

$memcache = new Memcache();
$result2 = $memcache->connect($host, $port);
$result3 = $memcache->set('test_key', 'test', false, 1);
$result4 = $memcache->close();

// This will fail since all servers have been removed
$result5 = $memcache->get('test_key');

// Reconnect server
$result6 = $memcache->connect($host, $port);
$result7 = $memcache->get('test_key');

var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump($result5);
var_dump($result6);
var_dump($result7);

?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
string(4) "test"
