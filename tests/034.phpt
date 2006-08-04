--TEST--
memcache->getStats() with arguments
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$result = $memcache->set('test_key', 'abc');
var_dump($result);

$result = $memcache->getStats();
var_dump(is_numeric($result['pid']));

$result = $memcache->getStats('abc');
var_dump($result);

$result = $memcache->getStats('reset');
var_dump($result);

$result = $memcache->getStats('malloc');
var_dump(is_numeric($result['arena_size']));

$result = $memcache->getStats('maps');
var_dump(count($result) > 0);

$result = $memcache->getStats('cachedump', 6, 10);
var_dump(count($result) > 0);

$result = $memcache->getStats('slabs');
var_dump($result['6']['chunk_size']);

$result = $memcache->getStats('items');
var_dump(isset($result['items']));

$result = $memcache->getStats('sizes');
var_dump(count($result) > 0);

print "\n";

$result = $memcache->getExtendedStats('abc');
var_dump($result["$host:$port"]);

$result = $memcache->getExtendedStats('items');
var_dump(isset($result["$host:$port"]['items']));

?>
--EXPECTF--
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
string(%d) "%d"
bool(true)
bool(true)

bool(false)
bool(true)
