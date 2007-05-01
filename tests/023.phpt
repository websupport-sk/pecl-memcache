--TEST--
memcache->delete() with load balancing
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$var1 = 'test1';
$var2 = 'test2';

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2);

$memcache1 = memcache_connect($host, $port);
$memcache2 = memcache_connect($host2, $port2);

$memcache1->set($balanceKey1, '', false, 2);
$memcache1->set($balanceKey2, '', false, 2);
$memcache2->set($balanceKey1, '', false, 2);
$memcache2->set($balanceKey2, '', false, 2);

$result1 = $memcache->set($balanceKey1, $var1, false, 2); 	// hashes to host2
$result2 = $memcache->set($balanceKey2, $var2, false, 2);	// hashes to host1
$result3 = $memcache->get($balanceKey1);
$result4 = $memcache->get($balanceKey2);

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);
print "\r\n";

$result5 = $memcache1->get($balanceKey1);
$result6 = $memcache1->get($balanceKey2);
$result7 = $memcache2->get($balanceKey1);
$result8 = $memcache2->get($balanceKey2);

var_dump($result5);
var_dump($result6);
var_dump($result7);
var_dump($result8);
print "\r\n";

$result9 = $memcache->delete($balanceKey1);
$result10 = $memcache->get($balanceKey1);
$result11 = $memcache2->get($balanceKey1);

var_dump($result9);
var_dump($result10);
var_dump($result11);

$result12 = $memcache->delete($balanceKey2);
$result13 = $memcache->get($balanceKey2);
$result14 = $memcache1->get($balanceKey2);

var_dump($result12);
var_dump($result13);
var_dump($result14);

?>
--EXPECT--
bool(true)
bool(true)
string(5) "test1"
string(5) "test2"

string(0) ""
string(5) "test2"
string(5) "test1"
string(0) ""

bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
