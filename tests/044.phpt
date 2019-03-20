--TEST--
ini_set('memcache.session_redundancy')
--SKIPIF--
<?php
include 'connect.inc';
include 'version.inc';
if (defined('PHP_VERSION_ID') && !(PHP_VERSION_ID < 70300)) {
    die("skip");
}
?>

--FILE--
<?php
ob_start();

include 'connect.inc';

ini_set('session.use_strict_mode', 0);
ini_set('memcache.session_redundancy', 2);
ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', "tcp://$host:$port?udp_port=$udpPort, tcp://$host2:$port2?udp_port=$udpPort2");

$memcache1 = test_connect1();
$memcache2 = test_connect2();
$memcache1->delete($balanceKey1);
$memcache2->delete($balanceKey1);

// Test set
session_id($balanceKey1);
session_start();
$_SESSION['key'] = 'Test1';
session_write_close();

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

// Test delete
session_id($balanceKey1);
@session_start();
session_destroy();

$result1 = $memcache1->get($balanceKey1);
$result2 = $memcache2->get($balanceKey1);
var_dump($result1);
var_dump($result2);

// Test lost session on server1
session_id($balanceKey1);
@session_start();
$_SESSION['key'] = 'Test2';
session_write_close();
unset($_SESSION['key']);

$result = $memcache1->delete($balanceKey1);
var_dump($result);

session_id($balanceKey1);
@session_start();
var_dump($_SESSION);
session_write_close();

// Test lost session on server2
session_id($balanceKey2);
@session_start();
$_SESSION['key'] = 'Test3';
session_write_close();
unset($_SESSION['key']);

$result = $memcache2->delete($balanceKey1);
var_dump($result);

session_id($balanceKey2);
@session_start();
var_dump($_SESSION);
session_write_close();
ob_flush();
?>
--EXPECTF--
string(16) "key%sTest1%s"
string(16) "key%sTest1%s"
bool(false)
bool(false)
bool(true)
array(1) {
  ["key"]=>
  string(5) "Test2"
}
bool(true)
array(1) {
  ["key"]=>
  string(5) "Test3"
}
