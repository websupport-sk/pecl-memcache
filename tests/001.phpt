--TEST--
memcache_set() function
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$value = new stdClass;
$value->plain_attribute = 'value';
$value->array_attribute = array('test1', 'test2');
memcache_set($memcache, 'test_key', $value, false, 10);

$key = 123;
$value = 123;
memcache_set($memcache, $key, $value, false, 10);

var_dump($key);
var_dump($value);

echo "Done\n";

?>
--EXPECT--
int(123)
int(123)
Done
