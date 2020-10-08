--TEST--
memcache_close(), memcache_get()
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80000)
    die('skip php 8+ only');
include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

memcache_add_server($memcache, $host2, $port2);
memcache_add_server($memcache, $nonExistingHost, $nonExistingPort);

$result1 = memcache_close($memcache);
var_dump($result1);

$memcache = new Memcache();
$result2 = $memcache->connect($host, $port);
$result3 = memcache_set($memcache, 'test_key', 'test', false, 1);
$result4 = memcache_close($memcache);

// This will fail since all servers have been removed
$result5 = memcache_get($memcache, 'test_key');

var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump($result5);


var_dump(memcache_close($memcache));
var_dump(memcache_close($memcache));
try {
    var_dump(memcache_close(new stdClass));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}
try {
    var_dump(memcache_close(""));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
memcache_close(): Argument #1 ($memcache) must be of type MemcachePool, stdClass given
memcache_close(): Argument #1 ($memcache) must be of type MemcachePool, string given
Done
