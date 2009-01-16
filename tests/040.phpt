--TEST--
memcache->increment()/decrement() with multiple keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache = test_connect_pool();
$memcache->set($balanceKey1, 1, 0, 10);
$memcache->set($balanceKey2, 2, 0, 10);

$result = $memcache->increment(array($balanceKey1, $balanceKey2), 1);
var_dump($result);

$result1 = $memcache->get($balanceKey1);
$result2 = $memcache->get($balanceKey2);
var_dump($result1);
var_dump($result2);

$result = $memcache->decrement(array($balanceKey1, $balanceKey2), 1);
var_dump($result);

$result = memcache_increment($memcache, array($balanceKey1, $balanceKey2), 1);
var_dump($result);

$result = memcache_decrement($memcache, array($balanceKey1, $balanceKey2), 1);
var_dump($result);

$result = $memcache->increment(array());
var_dump($result);

$result = $memcache->increment(array('unset_test_key', 'unset_test_key1'));
var_dump($result);

?>
--EXPECTF--
array(2) {
  ["%s"]=>
  int(2)
  ["%s"]=>
  int(3)
}
int(2)
int(3)
array(2) {
  ["%s"]=>
  int(1)
  ["%s"]=>
  int(2)
}
array(2) {
  ["%s"]=>
  int(2)
  ["%s"]=>
  int(3)
}
array(2) {
  ["%s"]=>
  int(1)
  ["%s"]=>
  int(2)
}
array(0) {
}
array(2) {
  ["%s"]=>
  bool(false)
  ["%s"]=>
  bool(false)
}
