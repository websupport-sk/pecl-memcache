--TEST--
Memcache session handler supports strict mode
--SKIPIF--
<?php
if (!extension_loaded('memcache')) {
    die('skip memcache extension not loaded');
}
if (PHP_VERSION_ID < 80600) {
    die('skip PHP 8.6+ only');
}
?>
--INI--
session.save_handler=memcache
session.save_path=tcp://127.0.0.1:11211
session.use_strict_mode=1
session.use_cookies=0
session.cache_limiter=
--FILE--
<?php

/* Create a session and write some data. */
session_start();
$sid = session_id();

$_SESSION['foo'] = 'bar';
$_SESSION['answer'] = 42;

session_write_close();

/* An existing SID must be accepted in strict mode. */
session_id($sid);
var_dump(session_start());
var_dump(session_id() === $sid);
var_dump($_SESSION);

session_write_close();

/* A nonexistent SID must be rejected in strict mode. */
$invalid = 'this-session-does-not-exist';

session_id($invalid);
var_dump(session_start());
var_dump(session_id() !== $invalid);

session_abort();

?>
--EXPECT--
bool(true)
bool(true)
array(2) {
  ["foo"]=>
  string(3) "bar"
  ["answer"]=>
  int(42)
}
bool(true)
bool(true)
