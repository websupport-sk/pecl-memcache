--TEST--
memcache_get() function
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

$result = memcache_set($memcache, 'test_key', $var);
var_dump($result);

$result = memcache_set($memcache, 'test_key1', $var);
var_dump($result);

$result = memcache_get($memcache, 'test_key');
var_dump($result);

$result = memcache_get($memcache, array('test_key', 'test_key1'));
var_dump($result);

$result = memcache_get($memcache, array('unset_test_key', 'unset_test_key1'));
var_dump($result);

?>
--EXPECTF--
bool(true)
bool(true)
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
array(2) {
  ["test_key"]=>
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
  ["test_key1"]=>
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
}
array(0) {
}