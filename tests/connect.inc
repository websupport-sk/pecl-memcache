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
//$host2 = "localhost";
//$port2 = 11212;

/* Start a server listening to a unix domain socket
 *
 * mkdir /var/run/memcached
 * chown memcached:memcached /var/run/memcached
 * memcached -d -u memcached -s /var/run/memcached/memcached.sock
 * chmod a+w /var/run/memcached/memcached.sock
 */
//$domainsocket = 'unix:///var/run/memcached/memcached.sock';

$nonExistingHost = "localhost";
$nonExistingPort = 11213;

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

$memcache = @memcache_connect($host, $port);

if (!$memcache) {
	die('skip Connection to memcached failed');
}

?>
