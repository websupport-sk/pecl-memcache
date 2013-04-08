--TEST--
memcache_add() & memcache_replace()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

$result1 = memcache_add($memcache, 'non_existing_test_key', $var, false, 1);
$result2 = memcache_replace($memcache, 'non_existing_test_key', $var, false, 10);

var_dump($result1);
var_dump($result2);

memcache_delete($memcache, 'non_existing_test_key');

?>
--EXPECT--
bool(true)
bool(true)
