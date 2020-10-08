--TEST--
PECL bug #16442 (memcache_set fail with integer value)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$memcache_obj = memcache_connect('localhost', 11211);
memcache_set($memcache_obj, 'test123112', 1, MEMCACHE_COMPRESSED, 30);
$ret = memcache_get($memcache_obj, 'test123112');
var_dump($ret);

?>
--EXPECTF--
int(1)
