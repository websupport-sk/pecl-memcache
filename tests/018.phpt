--TEST--
memcache_set() & memcache_add()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
error_reporting(E_ALL);

$result1 = memcache_set($memcache, 'non_existing_test_key', $var, false, 1);
$result2 = memcache_add($memcache, 'non_existing_test_key', $var, false, 1);

var_dump($result1);
var_dump($result2);

?>
--EXPECT--
bool(true)
bool(false)
