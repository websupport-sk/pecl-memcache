--TEST--
memcache_set() function
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 80000)
    die('skip php prior to 8 only');
include 'connect.inc';
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

try {
    memcache_set($memcache, $key);
} catch (ArgumentCountError $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $memcache->set($key);
} catch (ArgumentCountError $e) {
    echo "{$e->getMessage()}\n";
}

echo "Done\n";

?>
--EXPECTF--
int(123)
int(123)

Warning: %s parameter%s

Warning: %s parameter%s
Done