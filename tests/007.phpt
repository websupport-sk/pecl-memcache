--TEST--
memcache_get_version() & memcache_get_stats()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$version = memcache_get_version($memcache);

$stats = memcache_get_stats($memcache);

if ($version) {
	echo "version ok\n";
}

if ($stats) {
	echo "stats ok\n";
}

?>
--EXPECT--
version ok
stats ok
