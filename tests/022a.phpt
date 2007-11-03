--TEST--
memcache_get_extended_stats()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); if (ini_get('memcache.protocol') == 'binary') die('skip binary protocol does not support stats'); ?>
--FILE--
<?php

include 'connect.inc';

$memcache = new Memcache();
memcache_add_server($memcache, $nonExistingHost, $nonExistingPort);
memcache_add_server($memcache, $host, $port);
memcache_add_server($memcache, $host2, $port2);

$result1 = @memcache_get_stats($memcache);
$result2 = @memcache_get_extended_stats($memcache);
var_dump(memcache_get_extended_stats(array()));
var_dump(memcache_get_extended_stats(new stdClass));

var_dump($result1['pid']);

var_dump(count($result2));
var_dump($result2["$host:$port"]['pid']);
var_dump($result2["$host2:$port2"]['pid']);
var_dump($result2["$nonExistingHost:$nonExistingPort"]);

?>
--EXPECTF--
Warning: memcache_get_extended_stats() expects parameter 1 to be MemcachePool, array given in %s on line %d
NULL

Warning: memcache_get_extended_stats() expects parameter 1 to be MemcachePool, object given in %s on line %d
NULL
string(%d) "%d"
int(3)
string(%d) "%d"
string(%d) "%d"
bool(false)
