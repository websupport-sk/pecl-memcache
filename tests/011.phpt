--TEST--
memcache->set()/memcache->get() using compression
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = new stdClass;
$var->plain_attribute = 'value';
$var->array_attribute = Array('test1', 'test2');

$memcache->set('test_key', $var, MEMCACHE_COMPRESSED, 10);

$result = $memcache->get('test_key');

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
