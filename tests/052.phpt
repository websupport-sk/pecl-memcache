--TEST--
memcache->connect() and memcache->close() in loop
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$start = time();
$count = 100;

$memcache = new Memcache();
$memcache->connect($host, $port, false);

$key = 'test_key';
$memcache->set($key, 'test');
$memcache->close();

for ($i=0; $i<$count; $i++) {
	$memcache->connect($host, $port, false);
	$result = $memcache->get($key);
	
	if (!$result) {
		printf('Failed to fetch value for iteration %d', $i);
	}
	
	$memcache->close();
}

$end = time();
printf("%d seconds", $end - $start);

?>
--EXPECT--
0 seconds