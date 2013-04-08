--TEST--
memcache->delete() with load balancing
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
$memcache->addServer($host2, $port2);

$result1 = $memcache->set('delete_fail_key1', $var, false, 1);
$result2 = $memcache->delete('delete_fail_key1');
$result3 = $memcache->delete('delete_fail_key2');

var_dump($result1);
var_dump($result2);
var_dump($result3);

$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort);
$result4 = @$memcache->delete('delete_fail_key1');

var_dump($result4);

?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
