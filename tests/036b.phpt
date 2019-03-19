--TEST--
ini_set('session.save_path')
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; ?>
--FILE--
<?php

include 'connect.inc';

$session_save_path = "tcp://$host:$port?persistent=1&udp_port=0&weight=2&timeout=2&retry_interval=10,tcp://$host2:$port2";
ini_set('session.save_handler', 'memcache');
session_save_path($session_save_path);


$result1 = session_start();
$id = session_id();

$_SESSION['_test_key'] = 'Test';

$result2 = $memcache->get($id);
session_write_close();
$result3 = $memcache->get($id);

// Test destroy
$result4 = session_start();
$result5 = session_destroy();
$result6 = $memcache->get($id);

// Test large session
$session_save_path = "tcp://$host:$port";
session_save_path($session_save_path);

session_start();
$largeval = str_repeat('a', 1024*2048);
$_SESSION['_test_key']= $largeval;
session_write_close();

// test large cookie lifetime
ini_set('session.gc_maxlifetime', 1209600);
$result7 = session_start();
$id = session_id();
$_SESSION['_test_key'] = 'Test';
$result8 = $memcache->get($id);
session_write_close();
$result9 = $memcache->get($id);


var_dump($result1);
var_dump($id);
var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump($result5);
var_dump($result6);
var_dump($result7);
var_dump($result8);
var_dump($result9);

?>
--EXPECTF--
bool(true)
string(%d) "%s"
bool(false)
string(%d) "%s"
bool(true)
bool(true)
bool(false)
bool(true)
string(%d) "%s"
string(%d) "%s"
