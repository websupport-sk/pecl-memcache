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
var_dump($result['pid']);

$result = $memcache->getStats('abc');
var_dump($result);

$result = $memcache->getStats('reset');
var_dump($result);

$result = $memcache->getStats('malloc');
var_dump($result['arena_size']);

$result = $memcache->getStats('slabs');
var_dump($result[key($result)]['chunk_size']);
var_dump($result[key($result)]['free_chunks_end']);
$slab = key($result);

$result = $memcache->getStats('cachedump', $slab, 10);
var_dump($result[key($result)][0]);
var_dump($result[key($result)][1]);

$result = $memcache->getStats('items');
var_dump($result['items'][$slab]['number']);

//$result = $memcache->getStats('sizes');
//var_dump($result['64']);

print "\n";

$result = $memcache->getExtendedStats('abc');
var_dump($result["$host:$port"]);

$result = $memcache->getExtendedStats('items');
var_dump(isset($result["$host:$port"]['items']));

?>
--EXPECTF--
bool(true)
string(%d) "%d"
bool(false)
bool(true)
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"

bool(false)
bool(true)
