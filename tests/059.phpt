--TEST--
Reflection
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80000)
    die('skip php 8+ only');
--FILE--
<?php

echo new ReflectionClass('MemcachePool');
echo new ReflectionClass('Memcache');

echo new ReflectionFunction('memcache_connect');
echo new ReflectionFunction('memcache_pconnect');
echo new ReflectionFunction('memcache_add_server');
echo new ReflectionFunction('memcache_set_server_params');
echo new ReflectionFunction('memcache_set_failure_callback');
echo new ReflectionFunction('memcache_get_server_status');
echo new ReflectionFunction('memcache_get_version');
echo new ReflectionFunction('memcache_add');
echo new ReflectionFunction('memcache_set');
echo new ReflectionFunction('memcache_replace');
echo new ReflectionFunction('memcache_cas');
echo new ReflectionFunction('memcache_append');
echo new ReflectionFunction('memcache_prepend');
echo new ReflectionFunction('memcache_get');
echo new ReflectionFunction('memcache_delete');
echo new ReflectionFunction('memcache_debug');
echo new ReflectionFunction('memcache_get_stats');
echo new ReflectionFunction('memcache_get_extended_stats');
echo new ReflectionFunction('memcache_set_compress_threshold');
echo new ReflectionFunction('memcache_increment');
echo new ReflectionFunction('memcache_decrement');
echo new ReflectionFunction('memcache_close');
echo new ReflectionFunction('memcache_flush');
echo new ReflectionFunction('memcache_set_sasl_auth_data');

--EXPECT--
Class [ <internal:memcache> class MemcachePool ] {

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [0] {
  }

  - Methods [23] {
    Method [ <internal:memcache> public method connect ] {

      - Parameters [7] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $tcp_port = <default> ]
        Parameter #2 [ <optional> $udp_port = <default> ]
        Parameter #3 [ <optional> $persistent = <default> ]
        Parameter #4 [ <optional> $weight = <default> ]
        Parameter #5 [ <optional> $timeout = <default> ]
        Parameter #6 [ <optional> $retry_interval = <default> ]
      }
    }

    Method [ <internal:memcache> public method addserver ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $tcp_port = <default> ]
        Parameter #2 [ <optional> $udp_port = <default> ]
        Parameter #3 [ <optional> $persistent = <default> ]
        Parameter #4 [ <optional> $weight = <default> ]
        Parameter #5 [ <optional> $timeout = <default> ]
        Parameter #6 [ <optional> $retry_interval = <default> ]
        Parameter #7 [ <optional> $status = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method setserverparams ] {

      - Parameters [6] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port = <default> ]
        Parameter #2 [ <optional> float $timeout = <default> ]
        Parameter #3 [ <optional> int $retry_interval = <default> ]
        Parameter #4 [ <optional> bool $status = <default> ]
        Parameter #5 [ <optional> $failure_callback = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method setfailurecallback ] {

      - Parameters [1] {
        Parameter #0 [ <required> ?callable $failure_callback ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method getserverstatus ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port = <default> ]
      }
      - Return [ int|bool ]
    }

    Method [ <internal:memcache> public method findserver ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $key ]
      }
      - Return [ string|bool ]
    }

    Method [ <internal:memcache> public method getversion ] {

      - Parameters [0] {
      }
      - Return [ string|bool ]
    }

    Method [ <internal:memcache> public method add ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method set ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method replace ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method cas ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method append ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method prepend ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method get ] {

      - Parameters [3] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed &$flags = <default> ]
        Parameter #2 [ <optional> mixed &$cas = <default> ]
      }
      - Return [ mixed ]
    }

    Method [ <internal:memcache> public method delete ] {

      - Parameters [2] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache> public method getstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type = <default> ]
        Parameter #1 [ <optional> int $slabid = <default> ]
        Parameter #2 [ <optional> int $limit = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache> public method getextendedstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type = <default> ]
        Parameter #1 [ <optional> int $slabid = <default> ]
        Parameter #2 [ <optional> int $limit = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache> public method setcompressthreshold ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $threshold ]
        Parameter #1 [ <optional> float $min_savings = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method increment ] {

      - Parameters [4] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $value = <default> ]
        Parameter #2 [ <optional> int $defval = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|int|bool ]
    }

    Method [ <internal:memcache> public method decrement ] {

      - Parameters [4] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $value = <default> ]
        Parameter #2 [ <optional> int $defval = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|int|bool ]
    }

    Method [ <internal:memcache> public method close ] {

      - Parameters [0] {
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method flush ] {

      - Parameters [1] {
        Parameter #0 [ <optional> int $delay = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method setSaslAuthData ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $username ]
        Parameter #1 [ <required> string $password ]
      }
      - Return [ bool ]
    }
  }
}
Class [ <internal:memcache> class Memcache extends MemcachePool ] {

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [0] {
  }

  - Methods [24] {
    Method [ <internal:memcache, overwrites MemcachePool, prototype MemcachePool> public method connect ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $port = <default> ]
        Parameter #2 [ <optional> $timeout = <default> ]
        Parameter #3 [ <optional> $unused4 = <default> ]
        Parameter #4 [ <optional> $unused5 = <default> ]
        Parameter #5 [ <optional> $unused6 = <default> ]
        Parameter #6 [ <optional> $unused7 = <default> ]
        Parameter #7 [ <optional> $unugsed8 = <default> ]
      }
    }

    Method [ <internal:memcache> public method pconnect ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $port = <default> ]
        Parameter #2 [ <optional> $timeout = <default> ]
        Parameter #3 [ <optional> $unused4 = <default> ]
        Parameter #4 [ <optional> $unused5 = <default> ]
        Parameter #5 [ <optional> $unused6 = <default> ]
        Parameter #6 [ <optional> $unused7 = <default> ]
        Parameter #7 [ <optional> $unugsed8 = <default> ]
      }
    }

    Method [ <internal:memcache, overwrites MemcachePool, prototype MemcachePool> public method addserver ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $tcp_port = <default> ]
        Parameter #2 [ <optional> $persistent = <default> ]
        Parameter #3 [ <optional> $weight = <default> ]
        Parameter #4 [ <optional> $timeout = <default> ]
        Parameter #5 [ <optional> $retry_interval = <default> ]
        Parameter #6 [ <optional> $status = <default> ]
        Parameter #7 [ <optional> $failure_callback = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setserverparams ] {

      - Parameters [6] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port = <default> ]
        Parameter #2 [ <optional> float $timeout = <default> ]
        Parameter #3 [ <optional> int $retry_interval = <default> ]
        Parameter #4 [ <optional> bool $status = <default> ]
        Parameter #5 [ <optional> $failure_callback = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setfailurecallback ] {

      - Parameters [1] {
        Parameter #0 [ <required> ?callable $failure_callback ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getserverstatus ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port = <default> ]
      }
      - Return [ int|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method findserver ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $key ]
      }
      - Return [ string|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getversion ] {

      - Parameters [0] {
      }
      - Return [ string|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method add ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method set ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method replace ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method cas ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method append ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method prepend ] {

      - Parameters [5] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed $value = <default> ]
        Parameter #2 [ <optional> int $flags = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
        Parameter #4 [ <optional> int $cas = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method get ] {

      - Parameters [3] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> mixed &$flags = <default> ]
        Parameter #2 [ <optional> mixed &$cas = <default> ]
      }
      - Return [ mixed ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method delete ] {

      - Parameters [2] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type = <default> ]
        Parameter #1 [ <optional> int $slabid = <default> ]
        Parameter #2 [ <optional> int $limit = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getextendedstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type = <default> ]
        Parameter #1 [ <optional> int $slabid = <default> ]
        Parameter #2 [ <optional> int $limit = <default> ]
      }
      - Return [ array|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setcompressthreshold ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $threshold ]
        Parameter #1 [ <optional> float $min_savings = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method increment ] {

      - Parameters [4] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $value = <default> ]
        Parameter #2 [ <optional> int $defval = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|int|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method decrement ] {

      - Parameters [4] {
        Parameter #0 [ <required> array|string $key ]
        Parameter #1 [ <optional> int $value = <default> ]
        Parameter #2 [ <optional> int $defval = <default> ]
        Parameter #3 [ <optional> int $exptime = <default> ]
      }
      - Return [ array|int|bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method close ] {

      - Parameters [0] {
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method flush ] {

      - Parameters [1] {
        Parameter #0 [ <optional> int $delay = <default> ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setSaslAuthData ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $username ]
        Parameter #1 [ <required> string $password ]
      }
      - Return [ bool ]
    }
  }
}
Function [ <internal:memcache> function memcache_connect ] {

  - Parameters [8] {
    Parameter #0 [ <required> $host ]
    Parameter #1 [ <optional> $port = <default> ]
    Parameter #2 [ <optional> $timeout = <default> ]
    Parameter #3 [ <optional> $unused4 = <default> ]
    Parameter #4 [ <optional> $unused5 = <default> ]
    Parameter #5 [ <optional> $unused6 = <default> ]
    Parameter #6 [ <optional> $unused7 = <default> ]
    Parameter #7 [ <optional> $unugsed8 = <default> ]
  }
}
Function [ <internal:memcache> function memcache_pconnect ] {

  - Parameters [8] {
    Parameter #0 [ <required> $host ]
    Parameter #1 [ <optional> $port = <default> ]
    Parameter #2 [ <optional> $timeout = <default> ]
    Parameter #3 [ <optional> $unused4 = <default> ]
    Parameter #4 [ <optional> $unused5 = <default> ]
    Parameter #5 [ <optional> $unused6 = <default> ]
    Parameter #6 [ <optional> $unused7 = <default> ]
    Parameter #7 [ <optional> $unugsed8 = <default> ]
  }
}
Function [ <internal:memcache> function memcache_add_server ] {

  - Parameters [10] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $host ]
    Parameter #2 [ <optional> $port = <default> ]
    Parameter #3 [ <optional> $tcp_port = <default> ]
    Parameter #4 [ <optional> $persistent = <default> ]
    Parameter #5 [ <optional> $weight = <default> ]
    Parameter #6 [ <optional> $timeout = <default> ]
    Parameter #7 [ <optional> $retry_interval = <default> ]
    Parameter #8 [ <optional> $status = <default> ]
    Parameter #9 [ <optional> $failure_callback = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set_server_params ] {

  - Parameters [7] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> string $host ]
    Parameter #2 [ <optional> int $tcp_port = <default> ]
    Parameter #3 [ <optional> float $timeout = <default> ]
    Parameter #4 [ <optional> int $retry_interval = <default> ]
    Parameter #5 [ <optional> bool $status = <default> ]
    Parameter #6 [ <optional> $failure_callback = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set_failure_callback ] {

  - Parameters [2] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> ?callable $failure_callback ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_get_server_status ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> string $host ]
    Parameter #2 [ <optional> int $tcp_port = <default> ]
  }
  - Return [ int|bool ]
}
Function [ <internal:memcache> function memcache_get_version ] {

  - Parameters [1] {
    Parameter #0 [ <required> MemcachePool $memcache ]
  }
  - Return [ string|bool ]
}
Function [ <internal:memcache> function memcache_add ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_replace ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_cas ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_append ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_prepend ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> mixed $value = <default> ]
    Parameter #3 [ <optional> int $flags = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
    Parameter #5 [ <optional> int $cas = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_get ] {

  - Parameters [4] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> &$flags = <default> ]
    Parameter #3 [ <optional> &$cas = <default> ]
  }
  - Return [ mixed ]
}
Function [ <internal:memcache> function memcache_delete ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $exptime = <default> ]
  }
  - Return [ array|bool ]
}
Function [ <internal:memcache> function memcache_debug ] {

  - Parameters [1] {
    Parameter #0 [ <required> $on_off ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_get_stats ] {

  - Parameters [4] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <optional> string $type = <default> ]
    Parameter #2 [ <optional> int $slabid = <default> ]
    Parameter #3 [ <optional> int $limit = <default> ]
  }
  - Return [ array|bool ]
}
Function [ <internal:memcache> function memcache_get_extended_stats ] {

  - Parameters [4] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <optional> string $type = <default> ]
    Parameter #2 [ <optional> int $slabid = <default> ]
    Parameter #3 [ <optional> int $limit = <default> ]
  }
  - Return [ array|bool ]
}
Function [ <internal:memcache> function memcache_set_compress_threshold ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> int $threshold ]
    Parameter #2 [ <optional> float $min_savings = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_increment ] {

  - Parameters [5] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> int $value = <default> ]
    Parameter #3 [ <optional> int $defval = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
  }
  - Return [ array|int|bool ]
}
Function [ <internal:memcache> function memcache_decrement ] {

  - Parameters [5] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> array|string $key ]
    Parameter #2 [ <optional> int $value = <default> ]
    Parameter #3 [ <optional> int $defval = <default> ]
    Parameter #4 [ <optional> int $exptime = <default> ]
  }
  - Return [ array|int|bool ]
}
Function [ <internal:memcache> function memcache_close ] {

  - Parameters [1] {
    Parameter #0 [ <required> MemcachePool $memcache ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_flush ] {

  - Parameters [2] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <optional> int $delay = <default> ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set_sasl_auth_data ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> string $username ]
    Parameter #2 [ <required> string $password ]
  }
  - Return [ bool ]
}