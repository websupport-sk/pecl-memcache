--TEST--
memcache->getExtendedStats()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort);
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2);

$result1 = @$memcache->getStats();
$result2 = @$memcache->getExtendedStats();

var_dump(count($result1));

var_dump(count($result2));
var_dump(count($result2["$host:$port"]));
var_dump(count($result2["$host2:$port2"]));
var_dump($result2["$nonExistingHost:$nonExistingPort"]);

?>
--EXPECT--
int(19)
int(3)
int(19)
int(19)
bool(false)
