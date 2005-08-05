--TEST--
Memcache class should be inheritable
--FILE--
<?php

class Test extends Memcache {
	function foo() {
		echo "foo\n";
	}
}

$t = new Test;
$t->connect("localhost");
$t->foo();

var_dump($t);

echo "Done\n";
?>
--EXPECTF--	
foo
object(Test)#%d (1) {
  ["connection"]=>
  resource(%d) of type (memcache connection)
}
Done
