--TEST--
memcache->set() with duplicate keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$result = $memcache->set('test_key', array('test'));
var_dump($result);

$result = $memcache->get(array('test_key', 'test_key'));
var_dump($result);

?>
--EXPECT--
bool(true)
array(1) {
  ["test_key"]=>
  array(1) {
    [0]=>
    string(4) "test"
  }
}
