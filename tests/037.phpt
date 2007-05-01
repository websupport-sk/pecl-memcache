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

@ini_set('memcache.hash_strategy', 'abs');
var_dump(ini_get('memcache.hash_strategy'));

?>
--EXPECTF--
string(8) "standard"
string(10) "consistent"
string(10) "consistent"