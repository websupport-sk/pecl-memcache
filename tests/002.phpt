--TEST--
memcache_get() function
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

memcache_set($memcache, 'test_key', $var, false, 10);

$result = memcache_get($memcache, 'test_key');

var_dump($result);

?>
--EXPECTF--
object(stdClass)#%d (2) {
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
