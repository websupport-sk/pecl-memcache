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

$memcache1->set($balanceKey1, '', false, 2);
$memcache1->set($balanceKey2, '', false, 2);
$memcache2->set($balanceKey1, '', false, 2);
$memcache2->set($balanceKey2, '', false, 2);

$result1 = $memcache->set($balanceKey1, $var1, false, 2);	// hashes to host2
$result2 = $memcache->set($balanceKey2, $var2, false, 2);	// hashes to host1
$result3 = $memcache->increment($balanceKey1);
$result4 = $memcache->increment($balanceKey2);

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

$result5 = $memcache1->get($balanceKey1);
$result6 = $memcache1->get($balanceKey2);
$result7 = $memcache2->get($balanceKey1);
$result8 = $memcache2->get($balanceKey2);

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
string(0) ""
string(2) "21"
string(2) "11"
string(0) ""
