--TEST--
memcache->set()/get() datatype preservation
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache->set('test_key', '1');
$result = $memcache->get('test_key');
var_dump($result);

$memcache->set('test_key', 1);
$result = $memcache->get('test_key');
var_dump($result);

$memcache->set('test_key', true);
$result = $memcache->get('test_key');
var_dump($result);

$memcache->set('test_key', false);
$result = $memcache->get('test_key');
var_dump($result);

$memcache->set('test_key', 1.1);
$result = $memcache->get('test_key');
var_dump($result);

$memcache->set('test_key', '1', 0x10);

?>
--EXPECTF--
string(1) "1"
int(1)
bool(true)
bool(false)
float(1.1)

Warning: MemcachePool::set(): The lowest two bytes of the flags %s