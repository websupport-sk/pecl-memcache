--TEST--
ini_set("memcache.chunk_size")
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$result1 = ini_set("memcache.chunk_size", 16384);
var_dump($result1);

$result2 = ini_set("memcache.chunk_size", 32768);
var_dump($result2);

$result3 = ini_get("memcache.chunk_size");
var_dump($result3);

$result4 = @ini_set("memcache.chunk_size", 0);
var_dump($result4);

$result5 = @ini_set("memcache.chunk_size", -1);
var_dump($result5);

?>
--EXPECTF--
string(%d) "%d"
string(5) "16384"
string(5) "32768"
string(5) "32768"
string(5) "32768"
