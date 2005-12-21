--TEST--
Memcache class should be inheritable
--SKIPIF--
<?php if(!extension_loaded("memcache")) print "skip"; ?>
--FILE--
<?php

class test extends Memcache {
	function foo() {
		echo "foo\n";
	}
}

$t = new test;
$t->connect("localhost");
$t->foo();

var_dump($t);

echo "Done\n";
?>
--EXPECTF--	
foo
object(test)%s%d) {
  ["connection"]=>
  resource(%d) of type (memcache connection)
}
Done
