--TEST--
memcache->addServer() and memcache->close()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

// Test for bug #6595

include 'connect.inc';

$memcache2 = new Memcache();

$result1 = $memcache2->addServer($host, $port, true, 50);
$result2 = $memcache2->close();

var_dump($result1);
var_dump($result2);

?>
--EXPECTF--
bool(true)
bool(true)
