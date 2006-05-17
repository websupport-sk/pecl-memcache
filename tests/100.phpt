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

$memcache->addServer($nonExistingHost, $nonExistingPort);

$result1 = @$memcache->flush();
var_dump($result1);

$memcache2 = new Memcache();
$memcache2->addServer($nonExistingHost, $nonExistingPort);

$result2 = @$memcache2->flush();
var_dump($result2);

?>
--EXPECT--
bool(true)
bool(false)
