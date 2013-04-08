--TEST--
memcache_delete()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

memcache_set($memcache, 'test', 'test');

memcache_delete($memcache, 'test', 10);

/* the item should be still there */
memcache_delete($memcache, 'test');

echo "Done\n";

?>
--EXPECT--
Done
