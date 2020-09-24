#!/bin/bash

export CFLAGS="-fstack-protector-strong -O2"
export CPPFLAGS="${CFLAGS}"
export LDFLAGS="-Wl,-O1 -Wl,--hash-style=both"

# Build extension
set -eux
cd /usr/src

if [[ -z "${LOCAL_DEV:-}" ]] 
then
    git clone https://github.com/websupport-sk/pecl-memcache.git
fi 

cd pecl-memcache;
[[ -n "${LOCAL_DEV}" ]] && phpize --clean
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
NO_INTERACTION=1 TEST_PHP_ARGS="--show-diff --keep-all -w fails.log" make test
