--TEST--
session locking
--SKIPIF--
<?php include 'connect.inc'; if (!MEMCACHE_HAVE_SESSION) print 'skip not compiled with session support'; else if (!function_exists('pcntl_fork')) print 'skip not compiled with pcntl_fork() support'; ?>
--FILE--
<?php

include 'connect.inc';

$childcount = 50;
$sid = md5(rand());

ini_set('session.save_handler', 'memcache');
ini_set('memcache.session_save_path', "tcp://$host:$port?udp_port=$udpPort;tcp://$host2:$host2?udp_port=$udpPort2;tcp://$nonExistingHost:$nonExistingPort");
ini_set('memcache.session_redundancy', 10);

$pids = array();
for ($i = 0; $i < $childcount; $i++) {
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
	else {
		// In parent process
		$pids[] = $pid;
	}
}

// Wait for children to exit
foreach ($pids as $pid) {
	$status = 0;
	pcntl_waitpid($pid, $status);
	if ($status != 0) {
		//print "Child exited abnormally ($status)\n";
	}
}

session_id($sid);
session_start();
var_dump($_SESSION);

?>
--EXPECT--
array(1) {
  ["counter"]=>
  int(50)
}
