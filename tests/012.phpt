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

$result1 = $memcache->add('non_existing_test_key2', $var, false, 1);
$result2 = $memcache->replace('non_existing_test_key2', $var, false, 10);

var_dump($result1);
var_dump($result2);

$memcache->delete('non_existing_test_key2');

?>
--EXPECT--
bool(true)
bool(true)
