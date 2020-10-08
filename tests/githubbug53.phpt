--TEST--
Module shouldn't crash on failed serialization
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php
include 'connect.inc';

class foo {
   function __sleep() {
       throw new \Exception("fail");
   }
}

$oFoo = new foo();
$memcache->set('foobar', $oFoo);

--EXPECTF--
Warning: MemcachePool::set(): Failed to serialize value in %s on line %d

Fatal error: Uncaught Exception: fail in %s:%d
Stack trace:
#0 [internal function]: foo->__sleep()
#1 %s(%d): MemcachePool->set('foobar', Object(foo))
#2 {main}
  thrown in %s on line %d
