--TEST--
memcache->set()/memcache->get() with failover
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$var1 = 'test1';
$var2 = 'test2';

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result1 = $memcache->set('load_test_key1', $var1, false, 1);
$result2 = $memcache->set('load_test_key2', $var2, false, 1);
$result3 = $memcache->get('load_test_key1');
$result4 = $memcache->get(array('load_test_key1','load_test_key2'));

var_dump($result1);
var_dump($result2);
var_dump($result3);
var_dump($result4);

$memcache = new Memcache();
$memcache->addServer($nonExistingHost, $nonExistingPort);

$result5 = @$memcache->set('load_test_key1', $var1, false, 1);
$result6 = @$memcache->get('load_test_key1');

var_dump($result5);
var_dump($result6);

?>
--EXPECT--
bool(true)
bool(true)
string(5) "test1"
array(2) {
  ["load_test_key1"]=>
  string(5) "test1"
  ["load_test_key2"]=>
  string(5) "test2"
}
bool(false)
bool(false)