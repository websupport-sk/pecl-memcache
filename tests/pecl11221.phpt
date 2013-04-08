--TEST--
PECL bug #11221 (Double free when returning cached object with __sleep)
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

class block
{
	var $waa = 'meukee';
	function __sleep() { return array('waa'); }
}

function cache_failure()
{
	$block = new block();
	var_dump($GLOBALS['cache']->set('block', $block));

	return $block;
}

$cache = new memcache;
$cache->connect($host, $port);

cache_failure();

echo "Done\n";
?>
--EXPECTF--	
bool(true)
Done
