--TEST--
memcache->setSaslAuthData() and memcache_set_sasl_auth_data()
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php
include 'connect.inc';

$memcache->setSaslAuthData('testuser1', 'testpassword1');

var_dump($memcache->username);
var_dump($memcache->password);

memcache_set_sasl_auth_data($memcache, 'testuser2', 'testpassword2');

var_dump($memcache->username);
var_dump($memcache->password);

--EXPECTF--
string(9) "testuser1"
string(13) "testpassword1"
string(9) "testuser2"
string(13) "testpassword2"