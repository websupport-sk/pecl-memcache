--TEST--
memcache->get() function
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

$result = $memcache->get('test_key');
var_dump($result);

$result = $memcache->get(array('unset_test_key', 'unset_test_key1'));
var_dump($result);

?>
--EXPECTF--
object(stdClass)%s2) {
  ["plain_attribute"]=>
  string(5) "value"
  ["array_attribute"]=>
  array(2) {
    [0]=>
    string(5) "test1"
    [1]=>
    string(5) "test2"
  }
}
array(0) {
}
