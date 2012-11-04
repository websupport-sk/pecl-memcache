--TEST--
PECL bug #63272 (Explicitly reserve range of flags in php_memcache.h so application code can use)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$user_flags = array(
	MEMCACHE_USER1,
	MEMCACHE_USER2,
	MEMCACHE_USER3,
	MEMCACHE_USER4,
);

$m = new Memcache;
$m->addServer($host, $port);

foreach ($user_flags as $user_flag) {
	$m->set("testkey", "testvalue", $user_flag);
	$m->set("testkey_compressed", "testvalue", $user_flag | MEMCACHE_COMPRESSED);

	$getflags = 0;
	var_dump($m->get("testkey", $getflags));
	var_dump($m->get("testkey_compressed", $getflags));
}

echo "Done\n";
?>
--EXPECT--	
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
string(9) "testvalue"
Done
