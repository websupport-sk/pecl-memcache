<?php

/*
* Please change host & port to match your configuration 
*/

$host = "localhost";
$port = 11211;

// Start an extra server and uncomment to enable load-balancing and failover tests
//$host2 = "localhost";
//$port2 = 11212;

$nonExistingHost = "localhost";
$nonExistingPort = 11213;

$memcache = memcache_connect($host, $port);

if (!$memcache) {
	die('skip Connection to memcached failed');
}

?>
