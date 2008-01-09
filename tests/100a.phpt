--TEST--
memcache_flush()
--SKIPIF--
<?php include 'connect.inc'; ?>
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
var_dump(memcache_flush(new stdClass));
var_dump(memcache_flush(''));

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(false)
bool(false)
bool(true)

Warning: memcache_flush() expects parameter 1 to be MemcachePool, object given in %s on line %d
NULL

Warning: memcache_flush() expects parameter 1 to be MemcachePool, string given in %s on line %d
NULL
Done
