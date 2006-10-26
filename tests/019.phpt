--TEST--
memcache->addServer()
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';

$memcache = new Memcache();
$result1 = @$memcache->set('test_key', 'test');
$result2 = @$memcache->get('test_key');

var_dump($result1);
var_dump($result2);

$result1 = $memcache->addServer($host, $port, true, 1, 1, 15);
$result2 = $memcache->connect($host2, $port2);

$result3 = $memcache->set('non_existing_test_key', $var, false, 1);
$result4 = $memcache->get('non_existing_test_key');
$result5 = $memcache->getExtendedStats();

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump(count($result5));

?>
--EXPECT--
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
string(4) "test"
int(2)