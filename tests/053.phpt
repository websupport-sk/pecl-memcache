--TEST--
ini_set('session.save_handler') with unix domain socket
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; if (empty($domainsocket)) print 'skip $domainsocket not set'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache = new Memcache();
$memcache->connect($domainsocket, 0);

$session_save_path = ",  $domainsocket:0?weight=1, ";
ini_set('session.save_handler', 'memcache');
ini_set('session.save_path', $session_save_path);

$result1 = session_start();
$id = session_id();

$_SESSION['_test_key'] = 'Test';

$result2 = $memcache->get($id);
session_write_close();
$result3 = $memcache->get($id);

// Test destroy
session_start();
$result4 = session_destroy();
$result5 = $memcache->get($id);

var_dump($result1);
var_dump($id);
var_dump($result2);
var_dump($result3);
var_dump($result4);
var_dump($result5);

?>
--EXPECTF--
bool(true)
string(%d) "%s"
bool(false)
string(%d) "%s"
bool(true)
bool(false)