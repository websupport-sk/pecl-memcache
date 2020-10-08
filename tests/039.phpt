--TEST--
memcache->get() over multi-datagram UDP
--SKIPIF--
<?php include 'connect.inc'; if (empty($udpPort)) print 'skip UDP is not enabled in connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$key = 'test_key';
$keys = array();

// About 4 datagrams or so
for ($i=0; $i < $udpPacketSize/strlen($key)*3; $i++) {
	$memcache->set($key.$i, $key.'_value_'.$i, 0, 10);
	$keys[] = $key.$i;
}

$memcacheudp = new MemcachePool();
$memcacheudp->addServer($host, $nonExistingPort, $udpPort);

$result1 = $memcacheudp->get($keys);
ksort($result1);

var_dump(count($result1));
var_dump(count($result1) == count($keys));
var_dump(reset($result1));

$result2 = array_keys($result1);
var_dump(reset($result2));

?>
--EXPECTF--
int(%d)
bool(true)
string(16) "test_key_value_0"
string(9) "test_key0"