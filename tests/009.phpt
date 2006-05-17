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

$memcache->set('test_key', $var, false, 10);

echo "Done\n";

?>
--EXPECT--
Done
