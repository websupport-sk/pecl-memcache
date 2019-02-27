--TEST--
ini_set('memcache.session_redundancy')
--SKIPIF--
<?php
include 'connect.inc';
include 'version.inc';
print(PHP_VERSION_ID);
if (defined('PHP_VERSION_ID') && (PHP_VERSION_ID > 70200)) {
    die("skip");
}
?>

--FILE--
<?php
include 'connect.inc';

ini_set('memcache.session_redundancy', 2);
ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', "tcp://$host:$port?udp_port=$udpPort,tcp://$host2:$port2?udp_port=$udpPort2");

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
?>
--EXPECTF--
string(16) "key%sTest1%s"
string(16) "key%sTest1%s"
