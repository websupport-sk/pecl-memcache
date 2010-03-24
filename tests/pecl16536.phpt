--TEST--
PECL bug #16536 (Weight of 0 causes SegFault on memcache_add_server)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$mmc = new Memcache;
var_dump(@$mmc->addServer($host, $port, true, 0));
var_dump(@$mmc->set('TEST_KEY', 'test_value'));

echo "Done\n";
?>
--EXPECTF--	
bool(false)
bool(false)
Done
