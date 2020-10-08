--TEST--
memcache_set_compress_threshold()
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 80000)
    die('skip php prior to 8 only');
include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = str_repeat('abc', 5000);
memcache_set_compress_threshold($memcache, 10000);

$result1 = memcache_set($memcache, 'non_existing_test_key', $var, 0, 1);
$result2 = memcache_get($memcache, 'non_existing_test_key');

var_dump($result1);
var_dump(strlen($result2));

memcache_set_compress_threshold($memcache, 10000, 0);
$result3 = memcache_set($memcache, 'non_existing_test_key', $var, 0, 1);
memcache_set_compress_threshold($memcache, 10000, 1);
$result4 = memcache_set($memcache, 'non_existing_test_key', $var, 0, 1);

var_dump($result3);
var_dump($result4);

$result5 = memcache_set($memcache, 'non_existing_test_key', 'abc', MEMCACHE_COMPRESSED, 1);
$result6 = memcache_get($memcache, 'non_existing_test_key');

var_dump($result5);
var_dump($result6);

try {
    var_dump(memcache_set_compress_threshold(array(), 10000, 0));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}
var_dump(memcache_set_compress_threshold($memcache, -1, -1));
var_dump(memcache_set_compress_threshold($memcache, 1, -1));
var_dump(memcache_set_compress_threshold($memcache, -1, 1));
try {
    var_dump(memcache_set_compress_threshold(new stdClass, 1, 1));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}

echo "Done\n";

?>
--EXPECTF--
bool(true)
int(15000)
bool(true)
bool(true)
bool(true)
string(3) "abc"
Argument 1 passed to memcache_set_compress_threshold() must be an instance of MemcachePool, array given

Warning: memcache_set_compress_threshold()%s threshold must be a positive integer in %s on line %d
bool(false)

Warning: memcache_set_compress_threshold()%s min_savings must be a float in the 0..1 range in %s on line %d
bool(false)

Warning: memcache_set_compress_threshold()%s threshold must be a positive integer in %s on line %d
bool(false)
Argument 1 passed to memcache_set_compress_threshold() must be an instance of MemcachePool, instance of stdClass given
Done
