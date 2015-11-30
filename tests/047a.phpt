--TEST--
memcache->get() with flags (PHP 5 only)
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
<?php if (version_compare(phpversion(), '7', '>=')) die('skip requires PHP 5 '); ?>
--FILE--
<?php

include 'connect.inc';

$flag1 = 0x10000;

$memcache->set('test_key1', 'test1', $flag1);

$result1 = $memcache->get('test_key1', null);
var_dump($result1);

// Test procedural
$result1 = memcache_get($memcache, 'test_key1', null);
var_dump($result1);

?>
--EXPECT--
string(5) "test1"
string(5) "test1"
