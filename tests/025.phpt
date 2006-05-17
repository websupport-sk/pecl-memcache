--TEST--
memcache->increment() with load balancing
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$var1 = 10;
$var2 = 20;

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2);

$memcache1 = memcache_connect($host, $port);
$memcache2 = memcache_connect($host2, $port2);

$result1 = $memcache->set('increment_test_key1', $var1, false, 1);
$result2 = $memcache->set('increment_test_key2', $var2, false, 1);
$result3 = $memcache->increment('increment_test_key1');
$result4 = $memcache->increment('increment_test_key2');

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

$result5 = $memcache1->get('increment_test_key1');
$result6 = $memcache1->get('increment_test_key2');
$result7 = $memcache2->get('increment_test_key1');
$result8 = $memcache2->get('increment_test_key2');

var_dump($result5);
var_dump($result6);
var_dump($result7);
var_dump($result8);

?>
--EXPECT--
bool(true)
bool(true)
int(11)
int(21)
bool(false)
string(2) "21"
string(2) "11"
bool(false)
