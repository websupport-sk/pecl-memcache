--TEST--
memcache->addServer() with microsecond timeout
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

function microtime_float()
{
    list($usec, $sec) = explode(" ", microtime());
    return ((float)$usec + (float)$sec);
}

$memcache = new Memcache();
$memcache->addServer($unreachableHost, $unreachablePort, false, 1, 0.1);

$t1 = microtime_float();
$memcache->set('test_key', '1');
$t2 = microtime_float();

$t = $t2 - $t1;
var_dump($t);
var_dump($t > 0.01 && $t < 0.2);

$memcache = new MemcachePool();
$memcache->addServer($unreachableHost, $unreachablePort, 0, true, 1, 0.1);

$t1 = microtime_float();
$memcache->set('test_key', '1');
$t2 = microtime_float();

$t = $t2 - $t1;
var_dump($t);
var_dump($t > 0.01 && $t < 0.2);

$memcache = new MemcachePool();
$t1 = microtime_float();
$memcache->connect($unreachableHost, $unreachablePort, 0, false, 1, 0.1);
$t2 = microtime_float();

$t = $t2 - $t1;
var_dump($t);
var_dump($t > 0.01 && $t < 0.2);

$memcache = new MemcachePool();
$memcache->addServer($unreachableHost, $unreachablePort, 0, true, 1, 1);
$memcache->setServerParams($unreachableHost, $unreachablePort, 0.1);

$t1 = microtime_float();
$memcache->set('test_key', '1');
$t2 = microtime_float();

$t = $t2 - $t1;
var_dump($t);
var_dump($t > 0.01 && $t < 0.2);

?>
--EXPECTF--
Notice: MemcachePool::set(): %s
float(0.%d)
bool(true)

Notice: MemcachePool::set(): %s
float(0.%d)
bool(true)

Notice: MemcachePool::connect(): %s

Warning: MemcachePool::connect(): %s
float(0.%d)
bool(true)

Notice: MemcachePool::set(): %s
float(0.%d)
bool(true)
