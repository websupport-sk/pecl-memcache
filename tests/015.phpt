--TEST--
memcache->getVersion() & memcache->getStats()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$version = $memcache->getVersion();
var_dump($version);

$stats = $memcache->getStats();
if (ini_get('memcache.protocol') == 'binary') {
	var_dump($stats === false);
	var_dump(1);
}
else {
	var_dump(count($stats) > 10);
	var_dump(count($stats));
}

?>
--EXPECTF--
string(%d) "%s"
bool(true)
int(%d)
