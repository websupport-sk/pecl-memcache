--TEST--
Nested get's in __wakeup()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

class A {
	function __wakeup() {
		global $memcache;
		$result = $memcache->get('_test_key2');
		var_dump($result);
	}
}

$a1 = new A();

$memcache->set('_test_key1', $a1);
$memcache->set('_test_key2', 'Test2');

$a2 = $memcache->get('_test_key1');
var_dump($a2);

$result = $memcache->get(array('_test_key1', '_test_key2'));
ksort($result);
var_dump($result);

?>
--EXPECTF--
string(5) "Test2"
object(A)%s {
}
string(5) "Test2"
array(2) {
  ["_test_key1"]=>
  object(A)%s {
  }
  ["_test_key2"]=>
  string(5) "Test2"
}