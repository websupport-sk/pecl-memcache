<?php
error_reporting(E_ALL);

$count = 50;	// Keys per interation (multi-key operations do $count keys at a time)
$reps = 15000;	// Number of iterations

// hostname, tcp_port, udp_port
$hosts = array(
	array('localhost', 11211, 11211),
	array('localhost', 11212, 11212),
	);
$pools = array();

$tests1 = array(
	array('test_get_single', 'Single-Get'),
	array('test_get_multi', 'Multi-Get'),
	array('test_get_single_obj', 'Get-Object'),
	array('test_get_multi_obj', 'MulGet-Object'),
	);

$tests2 = array(
	array('test_set_single', 'Single-Set'),
	array('test_set_single_obj', 'Set-Object'),
	array('test_incr_single', 'Single-Incr'),
	array('test_delete_single', 'Single-Del'),
	);

$tests3 = array(
	array('test_set_multi', 'Multi-Set'),
	array('test_set_multi_obj', 'MulSet-Object'),
	array('test_incr_multi', 'Multi-Incr'),
	array('test_delete_multi', 'Multi-Del'),
	);

function run_tests($pools, $tests) {
	global $values, $ints, $count, $reps;
	
	printf('%16s', '');
	foreach ($tests as $test) {
		list ($function, $caption) = $test;
		printf('%-15s', $caption);
	}
	print "\n";

	foreach ($pools as $pool) {
		list ($memcache, $caption) = $pool;
		printf('%-16s', $caption);

		foreach ($tests as $test) {
			list ($function, $caption) = $test;

			foreach ($values as $key => $value) 
				$memcache->set($key, $value);
			foreach ($ints as $key => $value) 
				$memcache->set($key, $value);

			$ts = microtime(true);
			
			for ($i=0; $i<$reps; $i++) {
				$function($memcache);
			}
			
			$ts2 = microtime(true);

			printf('%-15u', round(($count*$reps) / ($ts2-$ts)));
		}

		print "\n";
	}

	print "\n";
}

// Create pools to run all tests against
$memcache1 = new Memcache();
$memcache1->connect($hosts[0][0], $hosts[0][1]);
$pools[] = array($memcache1, 'TCP (1 server)');

if (class_exists('MemcachePool')) {
	$memcache2 = new MemcachePool();
	$memcache2->connect($hosts[0][0], $hosts[0][1], $hosts[0][2], false);
	$pools[] = array($memcache2, 'UDP (1 server)');
}

if (count($hosts) > 1) {
	$memcache3 = new Memcache();
	foreach ($hosts as $h)
		$memcache3->connect($h[0], $h[1]);
	$pools[] = array($memcache3, sprintf('TCP (%d servers)', count($hosts)));

	if (class_exists('MemcachePool')) {
		$memcache4 = new MemcachePool();
		foreach ($hosts as $h)
			$memcache4->connect($h[0], $h[1], $h[2], false);
		$pools[] = array($memcache4, sprintf('UDP (%d servers)', count($hosts)));
	}
}

// Create values to work with
$values = array();
$ints = array();
$objects = array();

$key = 'test_key';
$key2 = 'test_key_int';
$key3 = 'test_key_obj';

for ($i=0; $i<$count; $i++) {
	$values[$key.$i] = $key.'_value_'.$i;
	$ints[$key2.$i] = $i;
	$objects[$key3.$i] = new TestClass(new TestClass(new TestClass()));
}

// Configuration
printf("memcache.hash_strategy:     %s\n", ini_get('memcache.hash_strategy'));
printf("memcache.chunk_size:        %u\n", ini_get('memcache.chunk_size'));
printf("servers:                    %d\n", count($hosts));
printf("iterations:                 %d\n", $reps);
printf("keys per iteration:         %d\n", $count);
print "\n";

$ts = time();
run_tests($pools, $tests1);
run_tests($pools, $tests2);

if (class_exists('MemcachePool'))
	run_tests($pools, $tests3);
$ts2 = time();

printf("total time: %d minutes, %d seconds\n", floor(($ts2 - $ts)/60), ($ts2 - $ts)%60);

// Tests
class TestClass {
	function __construct($obj = null) {
		$this->v1 = 'abc';
		$this->v2 = 123;
		$this->v3 = 123.123;
		$this->v4 = array('abc', 123);
		$this->obj = $obj;
	}
}

function test_set_single($memcache) {
	global $values;
	foreach ($values as $key => $value)
		$memcache->set($key, $value);
}

function test_set_multi($memcache) {
	global $values;
	$memcache->set($values);
}

function test_set_single_obj($memcache) {
	global $objects;
	foreach ($objects as $key => $value)
		$memcache->set($key, $value);
}

function test_set_multi_obj($memcache) {
	global $objects;
	$memcache->set($objects);
}

function test_get_single($memcache) {
	global $values;
	foreach (array_keys($values) as $key)
		$memcache->get($key);
}

function test_get_multi_obj($memcache) {
	global $objects;
	$memcache->get(array_keys($objects));
}

function test_get_single_obj($memcache) {
	global $objects;
	foreach (array_keys($objects) as $key)
		$memcache->get($key);
}

function test_get_multi($memcache) {
	global $values;
	$memcache->get(array_keys($values));
}

function test_delete_single($memcache) {
	global $values;
	foreach (array_keys($values) as $key)
		$memcache->delete($key);
}

function test_delete_multi($memcache) {
	global $values;
	$memcache->delete(array_keys($values));
}

function test_incr_single($memcache) {
	global $ints;
	foreach (array_keys($ints) as $key)
		$memcache->increment($key);
}

function test_incr_multi($memcache) {
	global $ints;
	$memcache->increment(array_keys($ints));
}
