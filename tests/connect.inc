<?php

if (!extension_loaded("memcache")) {
	die("skip");
}

error_reporting(E_ALL);

/*
* Please change host & port to match your configuration 
*/

$host = "localhost";
$port = 11211;
$udpPort = 11211;

$host2 = "localhost";
$port2 = 11212;
$udpPort2 = 11212;

//ini_set('memcache.hash_strategy', 'standard');
//ini_set('memcache.hash_function', 'fnv');
//ini_set('memcache.protocol', 'binary');

if (ini_get('memcache.protocol') == 'binary') {
	$udpPort = 0;
	$udpPort2 = 0;
}

/* Start a server listening to a unix domain socket
 *
 * mkdir /var/run/memcached
 * chown memcached:memcached /var/run/memcached
 * memcached -d -u memcached -s /var/run/memcached/memcached.sock
 * chmod a+w /var/run/memcached/memcached.sock
 */
$domainsocket = 'unix:///var/run/memcached/memcached.sock';

// A server which is guaranteed to fail immediatly
$nonExistingHost = "localhost";
$nonExistingPort = 11213;

// A server which times out when attempting to connect to
$unreachableHost = '10.254.254.254';
$unreachablePort = 11211;

$udpPacketSize = 1400;

$balanceKeys = array(
	'consistent'	=> array(
			'crc32'		=> array('key1_abc', 'key2_abcde'),
			'fnv'		=> array('key1_a', 'key2_2534534'),
			),
	'standard'		=> array(
			'crc32'		=> array('load_test_key1', 'load_test_key2'),
			'fnv'		=> array('key1_ab', 'key2_a'),
			),
	);

$strat = strtolower(ini_get('memcache.hash_strategy'));
$func = strtolower(ini_get('memcache.hash_function'));
list ($balanceKey1, $balanceKey2) = $balanceKeys[$strat][$func];

if (!isset($udpPort))
	$udpPort = 0;
if (!isset($udpPort2))
	$udpPort2 = 0;

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
