--TEST--
Memcache class should be inheritable
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

class test extends Memcache {
	function foo() {
		echo "foo\n";
	}
}

$t = new test;
$t->connect($host, $port);
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
