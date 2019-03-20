--TEST--
memcache multi host save path function
--SKIPIF--
<?php 
include 'connect.inc'; 
if (defined('PHP_VERSION_ID') && PHP_VERSION_ID > 70000) {
	    die("skip not relevant for php 7.1+
");
}
?>
--FILE--
<?php
ob_start();
include 'connect.inc';

$session_save_path = "tcp://$host:$port,tcp://$host2:$port2";
ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', $session_save_path);

session_start();

function test() {
        session_name('SID_bof');
        session_id('abcdef');
        if (!session_start()) {
                var_dump('session_start failed');
                exit(1);
        }
        var_dump($_SESSION);
        $_SESSION['bof.test'] = 42;
        session_write_close();
}

test();

echo "Done\n";
ob_flush();
?>
--EXPECTF--
array(1) {
  'bof.test' =>
  int(42)
}
array(1) {
  'bof.test' =>
  int(42)
}
array(1) {
  'bof.test' =>
  int(42)
}

Done
