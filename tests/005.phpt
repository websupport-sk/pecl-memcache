--TEST--
memcache_set() & memcache_get() with strange keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
$key = "test\r\n\0 - really strange key";

memcache_set($memcache, $key, $var, false, 10);
$result = memcache_get($memcache, $key);
var_dump($result);

$result = memcache_get($memcache, array($key, $key."1"));
var_dump(count($result));

if (is_array($result)) {
	$keys = array_keys($result);
	var_dump(strtr($keys[0], "\r\n\0", "___"));
}

?>
--EXPECTF--
string(4) "test"
int(1)
string(28) "test%skey"
