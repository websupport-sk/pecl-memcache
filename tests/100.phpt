--TEST--
memcache->flush()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

// This test must be run last or some concurrency problems will occur
// since the "flush_all" seems to be done async and therefore will 
// affect subsequent calls to set() done with a second or so.

include 'connect.inc';

// Test flush in future
$memcache = new Memcache();
$memcache->addServer($host, $port);

$result1 = $memcache->set('test_key', 'abc');
$result2 = $memcache->get('test_key');
var_dump($result1);
var_dump($result2);

$result = $memcache->flush(time()+3);
var_dump($result);

sleep(1);

$result = $memcache->get('test_key');
var_dump($result);

sleep(2);

$result = $memcache->get('test_key');
var_dump($result);

// Test partly failing flush
$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result = @$memcache->flush();
var_dump($result);

// Test failing flush
$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result = @$memcache->flush();
var_dump($result);

?>
--EXPECT--
bool(true)
string(3) "abc"
bool(true)
string(3) "abc"
bool(false)
bool(true)
bool(false)
