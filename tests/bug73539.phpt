--TEST--
memcache multi host save path function
--SKIPIF--
<?php 
include 'connect.inc';
?>
--FILE--
<?php
ob_start();
include 'connect.inc';

$session_save_path = "tcp://$host:$port,tcp://$host2:$port2";
ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', $session_save_path);

session_id('abcdef');
session_start();
$_SESSION['bof.test'] = 42;
session_write_close();

$_SESSION = array();

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

ini_set('memcache.session_save_path', "tcp://$host:$port");
test();
ini_set('memcache.session_save_path', "tcp://$host2:$port2");
test();
ini_set('memcache.session_save_path', "tcp://$host:$port, tcp://$host2:$port2");
test();

echo "Done\n";
ob_flush();
?>
--EXPECTF--
array(1) {
  ["bof.test"]=>
  int(42)
}
array(1) {
  ["bof.test"]=>
  int(42)
}
array(1) {
  ["bof.test"]=>
  int(42)
}
Done
