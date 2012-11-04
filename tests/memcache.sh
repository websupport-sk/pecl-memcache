#!/bin/bash

# basic script to launch memcached for testing
memcached -p11211 -U11211 -d
memcached -p11212 -U11212 -d

# if this fails, make sure your user has permissions
# to write to /var/run/memcached
memcached -s /var/run/memcached/memcached.sock -d
