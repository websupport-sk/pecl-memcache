--TEST--
memcache_add_server()
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 80000)
    die('skip php prior to 8 only');
include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

var_dump(memcache_add_server($memcache, $host2, $port2));
var_dump(memcache_add_server($memcache, $nonExistingHost, $nonExistingPort));

try {
    var_dump(memcache_add_server(new stdclass, $host2, $port2));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}

try {
    var_dump(memcache_add_server($memcache, new stdclass, array()));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(true)
Argument 1 passed to memcache_add_server() must be an instance of MemcachePool, instance of stdClass given

Warning: memcache_add_server() expects parameter 2 to be string, object given in %s on line %d
NULL
Done