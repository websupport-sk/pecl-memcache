--TEST--
memcache_increment() & memcache_decrement()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

memcache_set($memcache, 'new_test', 5);

$result1 = memcache_increment($memcache, 'new_test');
$result2 = memcache_decrement($memcache, 'new_test');
$result3 = memcache_get($memcache, 'new_test');

var_dump($result1);
var_dump($result2);
var_dump($result3);

?>
--EXPECT--
int(6)
int(5)
string(1) "5"
