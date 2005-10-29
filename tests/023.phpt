--TEST--
memcache->delete() with load balancing
--SKIPIF--
<?php include 'connect.inc'; if(!extension_loaded("memcache") || !isset($host2)) print "skip"; ?>
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

$result1 = $memcache->set('delete_test_key1', $var1, false, 1);
$result2 = $memcache->set('delete_test_key2', $var2, false, 1);
$result3 = $memcache->get('delete_test_key1');
$result4 = $memcache->get('delete_test_key2');

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

$result5 = $memcache1->get('delete_test_key1');
$result6 = $memcache1->get('delete_test_key2');
$result7 = $memcache2->get('delete_test_key1');
$result8 = $memcache2->get('delete_test_key2');

var_dump($result5);
var_dump($result6);
var_dump($result7);
var_dump($result8);

$result9 = $memcache->delete('delete_test_key1');
$result10 = $memcache->get('delete_test_key1');
$result11 = $memcache2->get('delete_test_key1');

var_dump($result9);
var_dump($result10);
var_dump($result11);

$result12 = $memcache->delete('delete_test_key2');
$result13 = $memcache->get('delete_test_key2');
$result14 = $memcache1->get('delete_test_key2');

var_dump($result12);
var_dump($result13);
var_dump($result14);

?>
--EXPECT--
bool(true)
bool(true)
string(5) "test1"
string(5) "test2"
string(5) "test1"
bool(false)
bool(false)
string(5) "test2"
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)