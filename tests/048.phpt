--TEST--
memcache->get(), set() with CAS
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$result = $memcache->set('test_key', 'a');
var_dump($result);

$cas = null;
$flags = null;
$result = $memcache->get('test_key', $flags, $cas);
var_dump($result);
var_dump($cas);

$result1 = $memcache->cas('test_key', 'b', 0, 0, $cas + 1);
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

$result1 = $memcache->cas('test_key', 'b', 0, 0, $cas);
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

?>
--EXPECTF--
bool(true)
string(1) "a"
int(%s%d)
bool(false)
string(1) "a"
bool(true)
string(1) "b"
