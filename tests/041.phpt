--TEST--
memcache->delete() with multiple keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache = test_connect_pool();
$memcache->set($balanceKey1, 'abc');
$memcache->set($balanceKey2, 'def');

$result1 = $memcache->get($balanceKey1);
$result2 = $memcache->get($balanceKey2);
var_dump($result1);
var_dump($result2);

$result = $memcache->delete(array($balanceKey1, $balanceKey2));
var_dump($result);

$result1 = $memcache->get($balanceKey1);
$result2 = $memcache->get($balanceKey2);
var_dump($result1);
var_dump($result2);

print "\n";

$key = 'test_key';
$values = array();

for ($i=0; $i<250; $i++) {
	$values[$key.$i] = $key.'_value_'.$i;
}

$result1 = $memcache->set($values, null, 0, 10);
var_dump($result1);

$result2 = memcache_delete($memcache, array_keys($values));
var_dump($result1);

$result3 = $memcache->get(array_keys($values));
var_dump($result3);

?>
--EXPECT--
string(3) "abc"
string(3) "def"
bool(true)
bool(false)
bool(false)

bool(true)
bool(true)
array(0) {
}
