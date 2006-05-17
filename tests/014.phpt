--TEST--
memcache->increment() & memcache->decrement()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache->set('new_test', 5);

$result1 = $memcache->increment('new_test');
$result2 = $memcache->decrement('new_test');
$result3 = $memcache->get('new_test');

var_dump($result1);
var_dump($result2);
var_dump($result3);

?>
--EXPECT--
int(6)
int(5)
string(1) "5"
