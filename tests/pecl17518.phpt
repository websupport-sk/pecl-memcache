--TEST--
PECL bug #17518 (Strange behavior in increment on non integer and after)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$m = new Memcache();
$m->connect('localhost', 11211);

var_dump($m->increment('a', 42));
var_dump($m->get('a'));

$m->set('b', 'bar');
var_dump($m->increment('b', 42));
var_dump($m->get('b'));

$m->set('c', 1);
var_dump($m->increment('c', 42));
var_dump($m->get('c'));

$m->flush();
?>
--EXPECTF--	
bool(false)
bool(false)

Notice: MemcachePool::increment(): Server localhost (tcp 11211, udp 0) failed with: CLIENT_ERROR cannot increment or decrement non-numeric value
 (6) in %s/pecl17518.php on line 12
bool(false)
string(3) "bar"
int(43)
int(43)
