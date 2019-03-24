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
$memcache->connect($host, $port);

$key = 'test_key';
$memcache->set($key, 'test');
$memcache->close();

for ($i=0; $i<$count; $i++) {
	$memcache->connect($host, $port);
	$result = $memcache->get($key);
	
	if (!$result) {
		printf('Failed to fetch value for iteration %d', $i);
	}
	
	$memcache->close();
}

$end = time();
if (($end - $start) < 2) {
	echo "true";
} else {
	echo "false";
}

?>
--EXPECT--
true
