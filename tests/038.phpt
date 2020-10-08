--TEST--
memcache->get() over UDP
--SKIPIF--
skip known bug
<?php include 'connect.inc'; if (empty($udpPort)) print 'skip UDP is not enabled in connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$result1 = $memcache->set('test_key1', 'abc', 0, 10);
var_dump($result1);

$result2 = $memcache->set('test_key2', 'def', 0, 10);
var_dump($result2);

$memcacheudp = new MemcachePool();
$memcacheudp->addServer($host, $nonExistingPort, $udpPort);

$result3 = $memcacheudp->get(array('test_key1', 'test_key2'));
var_dump($result3);

$result4 = $memcacheudp->get(array('test_key1'));
var_dump($result4);

$result5 = $memcacheudp->get('test_key2');
var_dump($result5);

?>
--EXPECT--
bool(true)
bool(true)
array(2) {
  ["test_key1"]=>
  string(3) "abc"
  ["test_key2"]=>
  string(3) "def"
}
array(1) {
  ["test_key1"]=>
  string(3) "abc"
}
string(3) "def"