#!/bin/bash

export CFLAGS="-fstack-protector-strong -O2"
export CPPFLAGS="${CFLAGS}"
export LDFLAGS="-Wl,-O1 -Wl,--hash-style=both"

# Build extension
set -eux
cd /usr/src

git clone https://github.com/websupport-sk/pecl-memcache.git

cd pecl-memcache;
phpize
./configure 
make -j"$(nproc)"

# Spawn memcached for tests
echo "Starting memcached... "
mkdir -p /var/run/memcached
chown memcache:memcache /var/run/memcached
/usr/bin/memcached -m 64 -u memcache -s /var/run/memcached/memcached.sock -d
/usr/bin/memcached -m 64 -u memcache -U 11211 -l 127.0.0.1 -p 11211 -d
/usr/bin/memcached -m 64 -u memcache -U 11212 -l 127.0.0.1 -p 11212 -d

# Let's start tests
cd /usr/src/pecl-memcache
export NO_INTERACTION=1
export TEST_PHP_ARGS="--show-diff --keep-all -w fails.log"

MEMCACHE_PROTOCOL=ascii make test
MEMCACHE_PROTOCOL=binary make test
