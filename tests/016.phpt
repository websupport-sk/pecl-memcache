--TEST--
memcache->delete()
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

include 'connect.inc';

$memcache->set('test', 'test');

$memcache->delete('test', 10);

/* the item should be still there */
$memcache->delete('test');

echo "Done\n";

?>
--EXPECT--
Done
