<?php

if (!extension_loaded("memcache")) {
	die("skip");
}

/*
* Please change host & port to match your configuration 
*/

$host = "localhost";
$port = 11211;
$udpPort = 11211;

$host2 = "localhost";
$port2 = 11212;
$udpPort2 = 11212;

/* Start a server listening to a unix domain socket
 *
 * mkdir /var/run/memcached
 * chown memcached:memcached /var/run/memcached
 * memcached -d -u memcached -s /var/run/memcached/memcached.sock
 * chmod a+w /var/run/memcached/memcached.sock
 */
$domainsocket = 'unix:///var/run/memcached/memcached.sock';

$nonExistingHost = "localhost";
$nonExistingPort = 11213;

$udpPacketSize = 1400;

switch (strtolower(ini_get('memcache.hash_strategy'))) {
	case 'consistent':
		// with preloaded buckets
		$balanceKey1 = 'key1_abc';
		$balanceKey2 = 'key2_abcde';
		
		// without preloaded buckets
		//$balanceKey1 = 'key1_abcde';
		//$balanceKey2 = 'key2_abcdefg';
		break;

	default:
		$balanceKey1 = 'load_test_key1';	// Must hash to host1
		$balanceKey2 = 'load_test_key2';	// Must hash to host2
}

$memcache = memcache_connect($host, $port);

function test_connect1() {
	global $host, $port, $udpPort;
	$memcache = new MemcachePool();
	$memcache->connect($host, $port, isset($udpPort) ? $udpPort : 0);
	return $memcache;
}

function test_connect2() {
	global $host2, $port2, $udpPort2;
	$memcache = new MemcachePool();
	$memcache->connect($host2, $port2, isset($udpPort2) ? $udpPort2 : 0);
	return $memcache;
}

function test_connect_pool() {
	global $host, $port, $udpPort, $host2, $port2, $udpPort2;
	$memcache = new MemcachePool();
	$memcache->addServer($host, $port, isset($udpPort) ? $udpPort : 0);
	$memcache->addServer($host2, $port2, isset($udpPort2) ? $udpPort2 : 0);
	return $memcache;
}

if (!$memcache) {
	die('skip Connection to memcached failed');
}

?>
