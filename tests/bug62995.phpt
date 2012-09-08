--TEST--
Bug #62995 (memcache client Segmentation fault)
--SKIPIF--
<?php include 'connect.inc'; if (version_compare(phpversion(), '5.0.0', '<')) die('skip requires PHP >= 5.0.0'); ?>
--INI--
;fix me later
report_memleaks=0
--FILE--
<?php

class foo {

    private $memcache = null;

    public function get_memcache() {
        if($this->memcache == null) {
            $tmp = new Memcache();
            $tmp->addServer('localhost', 11211, 1, 1, 1, 15, true, array(
                $this,
                'fail'
            ));
            $tmp->setCompressThreshold(8192, 0.2);

            $this->memcache = $tmp;
        }

        return $this->memcache;
    }

    public function fail() {
    }
}
$foo = new foo();
$mem = $foo->get_memcache();
$foo = new foo();
$mem = $foo->get_memcache();


echo "okey";
--EXPECTF--
okey
