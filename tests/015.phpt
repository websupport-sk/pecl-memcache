--TEST--
memcache->getVersion() & memcache->getStats()
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$version = $memcache->getVersion();

$stats = $memcache->getStats();

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
