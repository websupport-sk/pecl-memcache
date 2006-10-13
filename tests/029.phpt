--TEST--
ini_set("memcache.allow_failover")
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var1 = 'test1';
$var2 = 'test2';

ini_set('memcache.allow_failover', 1);

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result1 = $memcache->set('load_test_key1', $var1, false, 1);	// hashes to $host2
$result2 = $memcache->set('load_test_key2', $var2, false, 1);	// hashes to $host1
$result3 = $memcache->get('load_test_key1');
$result4 = $memcache->get('load_test_key2');

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

$result = $memcache->get(array('load_test_key1','load_test_key2'));
var_dump($result);

ini_set('memcache.allow_failover', 0);

$result1 = $memcache->get('load_test_key1');
$result2 = $memcache->get('load_test_key2');

var_dump($result1);
var_dump($result2);

$result = $memcache->get(array('load_test_key1','load_test_key2'));
var_dump($result);

$result = ini_set('memcache.allow_failover', "abc");
var_dump($result);

?>
--EXPECTF--
bool(true)
bool(true)
string(5) "test1"
string(5) "test2"
array(2) {
  ["load_test_key1"]=>
  string(5) "test1"
  ["load_test_key2"]=>
  string(5) "test2"
}
bool(false)
string(5) "test2"
array(1) {
  ["load_test_key2"]=>
  string(5) "test2"
}
string(1) "0"
