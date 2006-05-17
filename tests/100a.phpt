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

memcache_add_server($memcache, $nonExistingHost, $nonExistingPort);

$result1 = @memcache_flush($memcache);
var_dump($result1);

$memcache2 = new Memcache();
$memcache2->addServer($nonExistingHost, $nonExistingPort);

$result2 = @memcache_flush($memcache2);
var_dump($result2);

memcache_close($memcache);
var_dump(memcache_flush($memcache));
var_dump(memcache_flush(new stdClass));
var_dump(memcache_flush(''));

echo "Done\n";

?>
--EXPECTF--
bool(true)
bool(false)

Notice: memcache_flush(): marked server '%s' as failed in %s on line %d
bool(true)

Warning: memcache_flush() expects parameter 1 to be Memcache, object given in %s on line %d
NULL

Warning: memcache_flush() expects parameter 1 to be Memcache, string given in %s on line %d
NULL
Done
