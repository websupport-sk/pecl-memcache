--TEST--
memcache->getStats() with arguments
--SKIPIF--
<?php include 'connect.inc'; if (ini_get('memcache.protocol') == 'binary') die('skip binary protocol does not support stats'); ?>
--FILE--
<?php

include 'connect.inc';

$result = $memcache->set('test_key', 'abc');
var_dump($result);

$result = $memcache->getStats();
var_dump($result['pid']);
$version = $result['version'];

$result = $memcache->getStats('abc');
var_dump($result);

$result = $memcache->getStats('reset');
var_dump($result);

// malloc was removed in memcached 1.4.0
if ($version >= '1.4.0') {
	var_dump("0");
} else {
	$result = $memcache->getStats('malloc');
	var_dump($result['arena_size']);
}

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

$result = $memcache->getExtendedStats('abc');
// adding "@" to suppress new behaviour in PHP 7.4+, see: https://wiki.php.net/rfc/notice-for-non-valid-array-container
@var_dump($result["$host:$port"]);

$result = $memcache->getExtendedStats('items');
var_dump(isset($result["$host:$port"]['items']));

?>
--EXPECTF--
bool(true)
string(%d) "%d"

Warning: %s: Invalid stats type %s
bool(false)
bool(true)
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"
string(%d) "%d"

Warning: %s: Invalid stats type %s
NULL
bool(true)
