<?php

/*
* Please change host & port to match your configuration 
*/

$host = "localhost";
$port = 11211;

$memcache = memcache_connect($host, $port);

if (!$memcache) {
	die('skip Connection to memcached failed');
}

?>
