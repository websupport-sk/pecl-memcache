--TEST--
memcache->setCompressThreshold()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = str_repeat('abc', 5000);
$memcache->setCompressThreshold(10000);

$result1 = $memcache->set('non_existing_test_key', $var, 0, 1);
$result2 = $memcache->get('non_existing_test_key');

var_dump($result1);
var_dump(strlen($result2));

$memcache->setCompressThreshold(10000, 0);
$result3 = $memcache->set('non_existing_test_key', $var, 0, 1);
$memcache->setCompressThreshold(10000, 1);
$result4 = $memcache->set('non_existing_test_key', $var, 0, 1);

var_dump($result3);
var_dump($result4);

$result5 = $memcache->set('non_existing_test_key', 'abc', MEMCACHE_COMPRESSED, 1);
$result6 = $memcache->get('non_existing_test_key');

var_dump($result5);
var_dump($result6);

?>
--EXPECT--
bool(true)
int(15000)
bool(true)
bool(true)
bool(true)
string(3) "abc"
