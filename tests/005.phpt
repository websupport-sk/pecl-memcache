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
$result = memcache_get($memcache, $key."1");

var_dump($result);

$result = memcache_get($memcache, Array($key, $key."1"));

var_dump($result);

?>
--EXPECT--
string(4) "test"
array(1) {
  ["test__"]=>
  string(4) "test"
}
