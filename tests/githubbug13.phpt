--TEST--
session_regenerate_id() should not cause fatal error
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; ?>
--FILE--
<?php
include 'connect.inc';

$session_save_path = "tcp://$host:$port,tcp://$host2:$port2";
ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', $session_save_path);

session_start();
var_dump(session_regenerate_id());

?>
--EXPECTF--
bool(true)