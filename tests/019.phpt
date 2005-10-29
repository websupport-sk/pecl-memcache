--TEST--
memcache->addServer()
--SKIPIF--
<?php include 'connect.inc'; if(!extension_loaded("memcache") || !isset($host2)) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2, true, 1, 1, 15);

$result1 = $memcache->set('non_existing_test_key', $var, false, 1);
$result2 = $memcache->get('non_existing_test_key');

var_dump($result1);
var_dump($result2);

?>
--EXPECT--
bool(true)
string(4) "test"
