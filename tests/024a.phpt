--TEST--
memcache_close(), memcache_get()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
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
var_dump(memcache_close(new stdClass));
var_dump(memcache_close(""));

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

Warning: memcache_close() expects parameter 1 to be Memcache, object given in %s on line %d
NULL

Warning: memcache_close() expects parameter 1 to be Memcache, string given in %s on line %d
NULL
Done
