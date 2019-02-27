--TEST--
ini_set('memcache.redundancy')
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

ini_set('memcache.redundancy', 1);
$memcache = test_connect_pool();
$memcache1 = test_connect1();
$memcache2 = test_connect2();

$memcache1->delete($balanceKey1);
$memcache2->delete($balanceKey1);

$result = $memcache->get($balanceKey1);
var_dump($result);

$memcache->set($balanceKey1, 'Test1');

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

ini_set('memcache.redundancy', 10);
$memcache = test_connect_pool();

// Test set
$memcache->set($balanceKey1, 'Test2');

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

// Test increment
$memcache->set($balanceKey1, '1');
$memcache->increment($balanceKey1);

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

// Test delete
$memcache->delete($balanceKey1);

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

// Test invalid values
ini_set('memcache.redundancy', 0);
ini_set('memcache.redundancy', -1);
ini_set('memcache.redundancy', 'Test');

?>
--EXPECTF--
bool(false)
bool(false)
string(5) "Test1"
string(5) "Test2"
string(5) "Test2"
string(1) "2"
string(1) "2"
bool(false)
bool(false)

Warning: ini_set(): memcache.redundancy must be a positive integer ('0' given) in %s

Warning: ini_set(): memcache.redundancy must be a positive integer ('-1' given) in %s

Warning: ini_set(): memcache.redundancy must be a positive integer ('Test' given) in %s
