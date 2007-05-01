--TEST--
ini_set('memcache.hash_strategy')
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

ini_set('memcache.hash_strategy', 'standard');
var_dump(ini_get('memcache.hash_strategy'));

ini_set('memcache.hash_strategy', 'consistent');
var_dump(ini_get('memcache.hash_strategy'));

ini_set('memcache.hash_strategy', 'abs');
var_dump(ini_get('memcache.hash_strategy'));

?>
--EXPECTF--
string(8) "standard"
string(10) "consistent"

Warning: ini_set(): memcache.hash_strategy must be in set {standard, consistent} ('abs' given) in /home/mikl/arch/php6/src/pecl/memcache.consistent/tests/037.php on line 11
string(10) "consistent"