--TEST--
memcache_flush()
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80000)
    die('skip php 8+ only');
include 'connect.inc'; ?>
--FILE--
<?php

// This test must be run last or some concurrency problems will occur
// since the "flush_all" seems to be done async and therefore will 
// affect subsequent calls to set() done with a second or so.

include 'connect.inc';

$result = @memcache_flush($memcache);
var_dump($result);

memcache_add_server($memcache, $nonExistingHost, $nonExistingPort);

$result = @memcache_flush($memcache);
var_dump($result);

$memcache2 = new Memcache();
$memcache2->addServer($nonExistingHost, $nonExistingPort);

$result = @memcache_flush($memcache2);
var_dump($result);

memcache_close($memcache);
var_dump(memcache_flush($memcache));
try {
    var_dump(memcache_flush(new stdClass));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}
try {
    var_dump(memcache_flush(''));
} catch (TypeError $e) {
    echo "{$e->getMessage()}\n";
}

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(false)
bool(false)
bool(true)
memcache_flush(): Argument #1 ($memcache) must be of type MemcachePool, stdClass given
memcache_flush(): Argument #1 ($memcache) must be of type MemcachePool, string given
Done
