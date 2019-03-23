#!/bin/bash

sudo mkdir -p /var/run/memcached
sudo chown memcache:memcache /var/run/memcached

phpize
./configure
make
make install

ls -la /var/run/memcached/
# Spawn memcached for tests
/usr/bin/memcached -m 64 -u memcache -l -s /var/run/memcached/memcached.sock -d
/usr/bin/memcached -m 64 -u memcache -U 11211 -l 127.0.0.1 -p 11211 -d
/usr/bin/memcached -m 64 -u memcache -U 11212 -l 127.0.0.1 -p 11212 -d

TEST_PHP_ARGS="--show-diff --keep-all -w fails.log" make test
