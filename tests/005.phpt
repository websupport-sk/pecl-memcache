--TEST--
memcache_set() & memcache_get() with strange keys
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
$key = "test\r\n\0 - really strange key";

memcache_set($memcache, $key, $var, false, 10);
$result = memcache_get($memcache, $key);

var_dump($result);

?>
--EXPECT--
string(4) "test"
