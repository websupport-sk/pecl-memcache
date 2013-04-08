--TEST--
memcache->set() with value larger than MTU
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; if (empty($domainsocket)) print 'skip $domainsocket not set'; ?>
--FILE--
<?php

include 'connect.inc';

ini_set('memcache.chunk_size', 32768);
$memcache->setCompressThreshold(1000000, 1.0);

$value = str_repeat('a', 64 * 1024);
$result = $memcache->set('test_key', $value, 0, 2);
var_dump($result);

$result2 = $memcache->get('test_key');
var_dump(strlen($result2));

?>
--EXPECTF--
bool(true)
int(65536)