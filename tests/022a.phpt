--TEST--
memcache_get_extended_stats()
--SKIPIF--
<?php include 'connect.inc'; if(!extension_loaded("memcache") || !isset($host2)) print "skip"; ?>
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

var_dump(count($result1));

var_dump(count($result2));
var_dump(count($result2["$host:$port"]));
var_dump(count($result2["$host2:$port2"]));
var_dump($result2["$nonExistingHost:$nonExistingPort"]);

?>
--EXPECTF--
Warning: memcache_get_extended_stats() expects parameter 1 to be Memcache, array given in %s on line %d
NULL

Warning: memcache_get_extended_stats() expects parameter 1 to be Memcache, object given in %s on line %d
NULL
int(19)
int(3)
int(19)
int(19)
bool(false)
