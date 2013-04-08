--TEST--
strange keys
--SKIPIF--
<?php include 'connect.inc'; ?>
--FILE--
<?php

include 'connect.inc';

$var = 'test';
$key = "test\r\n\0 - really strange key";

memcache_set($memcache, $key, $var, false);
$result = memcache_get($memcache, $key);
var_dump($result);

$result = memcache_get($memcache, $key."1");
var_dump($result);

$result = memcache_get($memcache, array($key, $key."1"));
var_dump(count($result));

if (is_array($result)) {
	$keys = array_keys($result);
	var_dump(strtr($keys[0], "\r\n\0", "___"));
}

// Test empty key
$result = memcache_set($memcache, '', 'test1');
var_dump($result);
$result = memcache_get($memcache, '');
var_dump($result);

memcache_set($memcache, 'test_key1', 'test1');
memcache_set($memcache, 'test_key2', 'test2');
$result = memcache_get($memcache, array('test_key1', '', 'test_key2'));
var_dump($result);

$result = memcache_get($memcache, '');
var_dump($result);
$result = memcache_increment($memcache, '');
var_dump($result);
$result = memcache_delete($memcache, '');
var_dump($result);

function test_key_oo($memcache, $key) {
	$memcache->set($key, '1');
	var_dump($key);
	$memcache->get($key);
	var_dump($key);
	$memcache->get(array($key, 'test_key1'));
	var_dump($key);
	$memcache->increment($key);
	var_dump($key);
	$memcache->decrement($key);
	var_dump($key);
	$memcache->delete($key);
	var_dump($key);
}

function test_key_proc($memcache, $key) {
	memcache_set($memcache, $key, '1');
	var_dump($key);
	memcache_get($memcache, $key);
	var_dump($key);
	memcache_get($memcache, array($key, 'test_key1'));
	var_dump($key);
	memcache_increment($memcache, $key);
	var_dump($key);
	memcache_decrement($memcache, $key);
	var_dump($key);
	memcache_delete($memcache, $key);
	var_dump($key);
}

// Test non-string key
$key = 123;

test_key_oo($memcache, $key);
print "\n";
test_key_proc($memcache, $key);
print "\n";

// Test modify key
$key = 'test test';

test_key_oo($memcache, $key);
print "\n";
test_key_proc($memcache, $key);
print "\n";

// Test UTF-8 key
$key = 'abcÿabc';

$result = $memcache->set($key, 'test1');
var_dump($result);

$result = $memcache->get($key);
var_dump($result);

$result = $memcache->get(array($key));
var_dump($result);

// Array keys
$memcache->set('test_key', 'value');
$result = $memcache->get(array('test_key'));
var_dump($result['test_key']);

// Long keys
var_dump(memcache_increment($memcache, 'forum_thread.php_55461ANDis_show=1ORDERBYis_topDESC,update_timeDESC_0', 1));

// Whitespace key
$values = array(' ' => 'value', 'test test' => 'value2');
$result = $memcache->set($values);
var_dump($result);

$keys = array_keys($values);
$result = $memcache->get($keys);
var_dump($keys);
var_dump(array_values($result));

?>
--EXPECTF--
string(4) "test"
bool(false)
int(1)
string(28) "%s"

Warning: memcache_set(): %s
bool(false)

Warning: memcache_get(): %s
bool(false)

Warning: memcache_get(): %s
array(2) {
  ["test_key1"]=>
  string(5) "test1"
  ["test_key2"]=>
  string(5) "test2"
}

Warning: memcache_get(): %s
bool(false)

Warning: memcache_increment(): %s
bool(false)

Warning: memcache_delete(): %s
bool(false)
int(123)
int(123)
int(123)
int(123)
int(123)
int(123)

int(123)
int(123)
int(123)
int(123)
int(123)
int(123)

string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"

string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"
string(9) "test test"

bool(true)
string(5) "test1"
array(1) {
  ["abcÿabc"]=>
  string(5) "test1"
}
string(5) "value"
bool(false)
bool(true)
array(2) {
  [0]=>
  string(1) " "
  [1]=>
  string(9) "test test"
}
array(2) {
  [0]=>
  string(5) "value"
  [1]=>
  string(6) "value2"
}
