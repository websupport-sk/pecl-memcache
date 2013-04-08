--TEST--
Nested get's in __wakeup()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

class testclass {
	function __wakeup() {
		global $memcache;
		$this->result = $memcache->get('_test_key3');
		var_dump($this->result);
	}
}

$a1 = new testclass();

$memcache->set('_test_key1', $a1);
$memcache->set('_test_key2', array(123));
$memcache->set('_test_key3', 'test3');

$a2 = $memcache->get('_test_key1');
var_dump($a2);

$result = $memcache->get(array('_test_key1', '_test_key2'));
if (is_array($result))
	ksort($result);
var_dump($result);

?>
--EXPECTF--
string(5) "test3"
object(testclass)%s {
  ["result"]=>
  string(5) "test3"
}
string(5) "test3"
array(2) {
  ["_test_key1"]=>
  object(testclass)%s {
    ["result"]=>
    string(5) "test3"
  }
  ["_test_key2"]=>
  array(1) {
    [0]=>
    int(123)
  }
}