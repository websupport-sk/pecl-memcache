--TEST--
memcache->getExtendedStats()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); if (ini_get('memcache.protocol') == 'binary') die('skip binary protocol does not support stats'); ?>
--FILE--
<?php

include 'connect.inc';

$memcache = new MemcachePool();
$memcache->addServer($nonExistingHost, $nonExistingPort);
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2);

$result1 = @$memcache->getStats();
$result2 = @$memcache->getExtendedStats();
$result3 = @$memcache->getExtendedStats();

var_dump($result1['pid']);

var_dump(count($result2));
var_dump($result2["$host:$port"]['pid']);
var_dump($result2["$host2:$port2"]['pid']);
var_dump($result2["$nonExistingHost:$nonExistingPort"]);

var_dump(count($result3));
var_dump($result3["$host:$port"]['pid']);
var_dump($result3["$host2:$port2"]['pid']);
var_dump($result3["$nonExistingHost:$nonExistingPort"]);

?>
--EXPECTF--
string(%d) "%d"
int(3)
string(%d) "%d"
string(%d) "%d"
bool(false)
int(3)
string(%d) "%d"
string(%d) "%d"
bool(false)
