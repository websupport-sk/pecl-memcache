--TEST--
PECL Bug #77900 (Memcache session handling, zend_mm_heap corrupted)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php
include 'connect.inc';

ini_set('session.save_handler', 'memcache');
ini_set('session.save_path', "tcp://$host:$port");
session_start();
$_SESSION['test']=true;
session_write_close();
session_start();
var_dump($_SESSION);
session_write_close();
--EXPECT--
array(1) {
  ["test"]=>
  bool(true)
}