--TEST--
Memcache class should be inheritable
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
object(test)%s(%d) {
  ["connection"]=>
  resource(%d) of type (memcache connection)
}
Done
