--TEST--
PECL bug #17566 (3.0.4 cache delete bug)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$m1 = new Memcache;
$m1->connect($host, $port);
$m1->set("test", "123");
$m1->delete("test");

$m2 = new Memcache;
$m2->connect($host, $port);
var_dump($m2->get("test"));

echo "Done\n";
?>
--EXPECTF--	
bool(false)
Done
