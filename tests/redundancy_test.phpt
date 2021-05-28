--TEST--
redundancy test
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; else if (!function_exists('pcntl_fork')) print 'skip not compiled with pcntl_fork() support'; ?>
--FILE--
<?php

include 'connect.inc';

$sid = md5(rand());

ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', "tcp://$host:$port,tcp://$host2:$port2");
ini_set('memcache.session_redundancy', 3);

$memcache1 = test_connect1();
$memcache2 = test_connect2();

$pid = pcntl_fork();
if (!$pid) {
    // In child process
    session_id($sid);
    session_start();
    if (!isset($_SESSION['counter']))
        $_SESSION['counter'] = 0;
    $_SESSION['counter'] += 1;
    session_write_close();

    exit(0);
}
pcntl_waitpid($pid, $status);

$memcache1->flush();

$pid = pcntl_fork();
if (!$pid) {
    // In child process
    session_id($sid);
    session_start();
    if (!isset($_SESSION['counter']))
        $_SESSION['counter'] = 0;
    $_SESSION['counter'] += 1;
    session_write_close();

    exit(0);
}
pcntl_waitpid($pid, $status);

$memcache2->flush();

$pid = pcntl_fork();
if (!$pid) {
    // In child process
    session_id($sid);
    session_start();
    if (!isset($_SESSION['counter']))
        $_SESSION['counter'] = 0;
    $_SESSION['counter'] += 1;
    session_write_close();

    exit(0);
}
pcntl_waitpid($pid, $status);


session_id($sid);
session_start();
var_dump($_SESSION);

?>
--EXPECT--
array(1) {
  ["counter"]=>
  int(3)
}
