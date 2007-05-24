--TEST--
memcache->set() method
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

$result = $memcache->set('test_key', $var, false, 10);
var_dump($result);

// Should generate a "SERVER_ERROR: out of memory"
$var = str_repeat('a', 1500000);
$memcache->setCompressThreshold(0);
$result = $memcache->set('test_key', $var, false, 10);
var_dump($result);

echo "Done\n";

?>
--EXPECT--
bool(true)
bool(false)
Done
