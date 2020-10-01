--TEST--
PECL bug #17518 (Strange behavior in increment on non integer and after)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$m = new Memcache();
$m->connect('localhost', 11211);

$m->delete('a');
$m->delete('b');
$m->delete('c');

var_dump($m->increment('a', 42));
var_dump($m->get('a'));

$m->set('b', 'bar');
var_dump($m->increment('b', 42));
var_dump($m->get('b'));

$m->set('c', 1);
var_dump($m->increment('c', 42));
var_dump($m->get('c'));

--EXPECTF--	
bool(false)
bool(false)

Notice: MemcachePool::increment(): Server localhost (tcp 11211, udp 0) failed with: %s (%d) in %s
bool(false)
string(3) "bar"
int(43)
int(43)
