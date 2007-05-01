--TEST--
memcache->set()/memcache->get() with multiple keys and load balancing
--SKIPIF--
<?php include 'connect.inc'; if (!isset($host2)) die('skip $host2 not set'); ?>
--FILE--
<?php

include 'connect.inc';

$var1 = 'test1';
$var2 = 'test2';

$memcache = new Memcache();
$memcache->addServer($host, $port);
$memcache->addServer($host2, $port2);

$memcache1 = memcache_connect($host, $port);
$memcache2 = memcache_pconnect($host2, $port2);

$memcache1->set($balanceKey1, '', false, 2);
$memcache1->set($balanceKey2, '', false, 2);
$memcache2->set($balanceKey1, '', false, 2);
$memcache2->set($balanceKey2, '', false, 2);

$result1 = $memcache->set($balanceKey1, $var1, false, 2);	// hashes to $host2
$result2 = $memcache->set($balanceKey2, $var2, false, 2);	// hashes to $host1
$result3 = $memcache->get(array($balanceKey1, $balanceKey2));
sort($result3);

var_dump($result1);
var_dump($result2);
var_dump($result3);

$result4 = $memcache1->get($balanceKey1);	// return false; key1 is at $host2 
$result5 = $memcache1->get($balanceKey2);
$result6 = $memcache2->get($balanceKey1);	
$result7 = $memcache2->get($balanceKey2);	// return false; key2 is at $host1

var_dump($result4);
var_dump($result5);
var_dump($result6);
var_dump($result7);

?>
--EXPECT--
bool(true)
bool(true)
array(2) {
  [0]=>
  string(5) "test1"
  [1]=>
  string(5) "test2"
}
string(0) ""
string(5) "test2"
string(5) "test1"
string(0) ""
