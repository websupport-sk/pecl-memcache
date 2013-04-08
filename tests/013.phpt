--TEST--
memcache->set() & memcache->get() with strange keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
$key = "test\r\n\0 - really strange key";

$memcache->set($key, $var, false, 10);
$result = $memcache->get($key);

var_dump($result);

?>
--EXPECT--
string(4) "test"
