--TEST--
memcache->set() with multiple values
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache = test_connect_pool();
$memcache->set($balanceKey1, '');
$memcache->set($balanceKey2, '');

$result1 = $memcache->get($balanceKey1);
$result2 = $memcache->get($balanceKey2);
var_dump($result1);
var_dump($result2);

$values = array(
	$balanceKey1 => 'abc',
	$balanceKey2 => 'def');

$result = $memcache->set($values);
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

$memcache->delete(array_keys($values));

$result1 = memcache_set($memcache, $values, null, 0, 10);
var_dump($result1);

$result2 = $memcache->get(array_keys($values));
ksort($result2);

var_dump(count($result2));
var_dump(count($result2) == count($values));
var_dump(reset($result2));

?>
--EXPECT--
string(0) ""
string(0) ""
bool(true)
string(3) "abc"
string(3) "def"

bool(true)
int(250)
bool(true)
string(16) "test_key_value_0"