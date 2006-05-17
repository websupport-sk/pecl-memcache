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
$result3 = $memcache->set('non_existing_test_key', 'test', false, 1);
$result4 = $memcache->close();

// This additional get() will transparently reconnect
$result5 = $memcache->get('non_existing_test_key');

var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump($result5);

?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
string(4) "test"
