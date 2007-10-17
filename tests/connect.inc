<?php

if (!extension_loaded("memcache")) {
	die("skip");
}

/*
* Please change host & port to match your configuration 
*/

$host = "localhost";
$port = 11211;

// Start an extra server and uncomment to enable load-balancing and failover tests
$host2 = "localhost";
$port2 = 11212;

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

$memcache = @memcache_connect($host, $port);

if (!$memcache) {
	die('skip Connection to memcached failed');
}

?>
