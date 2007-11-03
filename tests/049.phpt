--TEST--
memcache->append(), memcache->prepend()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$result1 = $memcache->set('test_key', 'a');
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

$result1 = $memcache->append('test_key', 'b');
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

$result1 = $memcache->prepend('test_key', 'c');
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

?>
--EXPECT--
bool(true)
string(1) "a"
bool(true)
string(2) "ab"
bool(true)
string(3) "cab"