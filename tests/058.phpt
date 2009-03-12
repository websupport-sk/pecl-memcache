--TEST--
memcache->findServer()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$result = $memcache->findServer('test_key');
var_dump($result);

?>
--EXPECTF--
string(%d) "%s:%d"