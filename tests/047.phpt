--TEST--
memcache->get() with flags
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$flag1 = 0x100;
$flag2 = 0x200;

$memcache->set('test_key1', 'test1', $flag1);
$memcache->set('test_key2', 'test2', $flag2);

// Test OO
$result1 = null;
$result2 = $memcache->get('test_key1', $result1);

var_dump($result1);
var_dump($result2);

$result3 = null;
$result4 = $memcache->get(array('test_key1', 'test_key2'), $result3);

if (is_array($result3))
	ksort($result3);
if (is_array($result4))
	ksort($result4);

var_dump($result3);
var_dump($result4);

// Test procedural
$result1 = null;
$result2 = memcache_get($memcache, 'test_key1', $result1);

var_dump($result1);
var_dump($result2);

$result3 = null;
$result4 = memcache_get($memcache, array('test_key1', 'test_key2'), $result3);

if (is_array($result3))
	ksort($result3);
if (is_array($result4))
	ksort($result4);

var_dump($result3);
var_dump($result4);

?>
--EXPECT--
int(256)
string(5) "test1"
array(2) {
  ["test_key1"]=>
  int(256)
  ["test_key2"]=>
  int(512)
}
array(2) {
  ["test_key1"]=>
  string(5) "test1"
  ["test_key2"]=>
  string(5) "test2"
}
int(256)
string(5) "test1"
array(2) {
  ["test_key1"]=>
  int(256)
  ["test_key2"]=>
  int(512)
}
array(2) {
  ["test_key1"]=>
  string(5) "test1"
  ["test_key2"]=>
  string(5) "test2"
}