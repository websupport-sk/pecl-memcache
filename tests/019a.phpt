--TEST--
memcache_add_server()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

var_dump(memcache_add_server($memcache, $host2, $port2));
var_dump(memcache_add_server($memcache, $nonExistingHost, $nonExistingPort));
var_dump(memcache_add_server(new stdclass, $host2, $port2));
var_dump(memcache_add_server($memcache, new stdclass, array()));

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(true)

Warning: memcache_add_server() expects parameter 1 to be Memcache, object given in %s on line %d
NULL

Warning: memcache_add_server() expects parameter 2 to be string, object given in %s on line %d
NULL
Done
