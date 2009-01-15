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

// Test default port
$memcache = new Memcache();

ini_set('memcache.default_port', $port);
$result1 = $memcache->addServer($host);

ini_set('memcache.default_port', $port2);
$result2 = $memcache->connect($host2);

$result3 = $memcache->set($balanceKey1, 'a');
$result4 = $memcache->set($balanceKey2, 'b');

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

// Test empty connection
$memcache = new Memcache();
$memcache->set('test_key', '1');

// Test bad connection id
$memcache = new Memcache();
$memcache->connect($host, $port);
var_dump($memcache->connection);

$memcache->connection = true;
$memcache->set('test_key', '1');

?>
--EXPECTF--
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
string(4) "test"
int(2)
bool(true)
bool(true)
bool(true)
bool(true)

Warning: %s::set(): No servers added %s
resource(%d) of type (memcache connection)

Warning: %s::set(): Invalid %s member variable %s
