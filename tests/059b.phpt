--TEST--
Reflection
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 80000)
    die('skip php prior to 8 only');
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
        Parameter #1 [ <optional> $tcp_port ]
        Parameter #2 [ <optional> $udp_port ]
        Parameter #3 [ <optional> $persistent ]
        Parameter #4 [ <optional> $weight ]
        Parameter #5 [ <optional> $timeout ]
        Parameter #6 [ <optional> $retry_interval ]
      }
    }

    Method [ <internal:memcache> public method addserver ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $tcp_port ]
        Parameter #2 [ <optional> $udp_port ]
        Parameter #3 [ <optional> $persistent ]
        Parameter #4 [ <optional> $weight ]
        Parameter #5 [ <optional> $timeout ]
        Parameter #6 [ <optional> $retry_interval ]
        Parameter #7 [ <optional> $status ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method setserverparams ] {

      - Parameters [6] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port ]
        Parameter #2 [ <optional> float $timeout ]
        Parameter #3 [ <optional> int $retry_interval ]
        Parameter #4 [ <optional> bool $status ]
        Parameter #5 [ <optional> $failure_callbac ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method setfailurecallback ] {

      - Parameters [1] {
        Parameter #0 [ <required> callable or NULL $failure_callback ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method getserverstatus ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port ]
      }
    }

    Method [ <internal:memcache> public method findserver ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $key ]
      }
    }

    Method [ <internal:memcache> public method getversion ] {

      - Parameters [0] {
      }
    }

    Method [ <internal:memcache> public method add ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method set ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method replace ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method cas ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method append ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method prepend ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method get ] {

      - Parameters [3] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> &$flags ]
        Parameter #2 [ <optional> &$cas ]
      }
    }

    Method [ <internal:memcache> public method delete ] {

      - Parameters [2] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache> public method getstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type ]
        Parameter #1 [ <optional> int $slabid ]
        Parameter #2 [ <optional> int $limit ]
      }
    }

    Method [ <internal:memcache> public method getextendedstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type ]
        Parameter #1 [ <optional> int $slabid ]
        Parameter #2 [ <optional> int $limit ]
      }
    }

    Method [ <internal:memcache> public method setcompressthreshold ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $threshold ]
        Parameter #1 [ <optional> float $min_savings ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method increment ] {

      - Parameters [4] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $value ]
        Parameter #2 [ <optional> int $defval ]
        Parameter #3 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache> public method decrement ] {

      - Parameters [4] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $value ]
        Parameter #2 [ <optional> int $defval ]
        Parameter #3 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache> public method close ] {

      - Parameters [0] {
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache> public method flush ] {

      - Parameters [1] {
        Parameter #0 [ <optional> int $delay ]
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
        Parameter #1 [ <optional> $port ]
        Parameter #2 [ <optional> $timeout ]
        Parameter #3 [ <optional> $unused4 ]
        Parameter #4 [ <optional> $unused5 ]
        Parameter #5 [ <optional> $unused6 ]
        Parameter #6 [ <optional> $unused7 ]
        Parameter #7 [ <optional> $unugsed8 ]
      }
    }

    Method [ <internal:memcache> public method pconnect ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $port ]
        Parameter #2 [ <optional> $timeout ]
        Parameter #3 [ <optional> $unused4 ]
        Parameter #4 [ <optional> $unused5 ]
        Parameter #5 [ <optional> $unused6 ]
        Parameter #6 [ <optional> $unused7 ]
        Parameter #7 [ <optional> $unugsed8 ]
      }
    }

    Method [ <internal:memcache, overwrites MemcachePool, prototype MemcachePool> public method addserver ] {

      - Parameters [8] {
        Parameter #0 [ <required> $host ]
        Parameter #1 [ <optional> $tcp_port ]
        Parameter #2 [ <optional> $persistent ]
        Parameter #3 [ <optional> $weight ]
        Parameter #4 [ <optional> $timeout ]
        Parameter #5 [ <optional> $retry_interval ]
        Parameter #6 [ <optional> $status ]
        Parameter #7 [ <optional> $failure_callback ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setserverparams ] {

      - Parameters [6] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port ]
        Parameter #2 [ <optional> float $timeout ]
        Parameter #3 [ <optional> int $retry_interval ]
        Parameter #4 [ <optional> bool $status ]
        Parameter #5 [ <optional> $failure_callbac ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setfailurecallback ] {

      - Parameters [1] {
        Parameter #0 [ <required> callable or NULL $failure_callback ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getserverstatus ] {

      - Parameters [2] {
        Parameter #0 [ <required> string $host ]
        Parameter #1 [ <optional> int $tcp_port ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method findserver ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $key ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getversion ] {

      - Parameters [0] {
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method add ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method set ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method replace ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method cas ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method append ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method prepend ] {

      - Parameters [5] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> $value ]
        Parameter #2 [ <optional> int $flags ]
        Parameter #3 [ <optional> int $exptime ]
        Parameter #4 [ <optional> int $cas ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method get ] {

      - Parameters [3] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> &$flags ]
        Parameter #2 [ <optional> &$cas ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method delete ] {

      - Parameters [2] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type ]
        Parameter #1 [ <optional> int $slabid ]
        Parameter #2 [ <optional> int $limit ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method getextendedstats ] {

      - Parameters [3] {
        Parameter #0 [ <optional> string $type ]
        Parameter #1 [ <optional> int $slabid ]
        Parameter #2 [ <optional> int $limit ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method setcompressthreshold ] {

      - Parameters [2] {
        Parameter #0 [ <required> int $threshold ]
        Parameter #1 [ <optional> float $min_savings ]
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method increment ] {

      - Parameters [4] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $value ]
        Parameter #2 [ <optional> int $defval ]
        Parameter #3 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method decrement ] {

      - Parameters [4] {
        Parameter #0 [ <required> $key ]
        Parameter #1 [ <optional> int $value ]
        Parameter #2 [ <optional> int $defval ]
        Parameter #3 [ <optional> int $exptime ]
      }
    }

    Method [ <internal:memcache, inherits MemcachePool> public method close ] {

      - Parameters [0] {
      }
      - Return [ bool ]
    }

    Method [ <internal:memcache, inherits MemcachePool> public method flush ] {

      - Parameters [1] {
        Parameter #0 [ <optional> int $delay ]
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
    Parameter #1 [ <optional> $port ]
    Parameter #2 [ <optional> $timeout ]
    Parameter #3 [ <optional> $unused4 ]
    Parameter #4 [ <optional> $unused5 ]
    Parameter #5 [ <optional> $unused6 ]
    Parameter #6 [ <optional> $unused7 ]
    Parameter #7 [ <optional> $unugsed8 ]
  }
}
Function [ <internal:memcache> function memcache_pconnect ] {

  - Parameters [8] {
    Parameter #0 [ <required> $host ]
    Parameter #1 [ <optional> $port ]
    Parameter #2 [ <optional> $timeout ]
    Parameter #3 [ <optional> $unused4 ]
    Parameter #4 [ <optional> $unused5 ]
    Parameter #5 [ <optional> $unused6 ]
    Parameter #6 [ <optional> $unused7 ]
    Parameter #7 [ <optional> $unugsed8 ]
  }
}
Function [ <internal:memcache> function memcache_add_server ] {

  - Parameters [10] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $host ]
    Parameter #2 [ <optional> $port ]
    Parameter #3 [ <optional> $tcp_port ]
    Parameter #4 [ <optional> $persistent ]
    Parameter #5 [ <optional> $weight ]
    Parameter #6 [ <optional> $timeout ]
    Parameter #7 [ <optional> $retry_interval ]
    Parameter #8 [ <optional> $status ]
    Parameter #9 [ <optional> $failure_callback ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set_server_params ] {

  - Parameters [7] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> string $host ]
    Parameter #2 [ <optional> int $tcp_port ]
    Parameter #3 [ <optional> float $timeout ]
    Parameter #4 [ <optional> int $retry_interval ]
    Parameter #5 [ <optional> bool $status ]
    Parameter #6 [ <optional> $failure_callbac ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set_failure_callback ] {

  - Parameters [2] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> callable or NULL $failure_callback ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_get_server_status ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> string $host ]
    Parameter #2 [ <optional> int $tcp_port ]
  }
}
Function [ <internal:memcache> function memcache_get_version ] {

  - Parameters [1] {
    Parameter #0 [ <required> MemcachePool $memcache ]
  }
}
Function [ <internal:memcache> function memcache_add ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_set ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_replace ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_cas ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_append ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_prepend ] {

  - Parameters [6] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $value ]
    Parameter #3 [ <optional> int $flags ]
    Parameter #4 [ <optional> int $exptime ]
    Parameter #5 [ <optional> int $cas ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_get ] {

  - Parameters [4] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <optional> $key ]
    Parameter #2 [ <optional> &$flags ]
    Parameter #3 [ <optional> &$cas ]
  }
}
Function [ <internal:memcache> function memcache_delete ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> $exptime ]
  }
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
    Parameter #1 [ <optional> string $type ]
    Parameter #2 [ <optional> int $slabid ]
    Parameter #3 [ <optional> int $limit ]
  }
}
Function [ <internal:memcache> function memcache_get_extended_stats ] {

  - Parameters [4] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <optional> string $type ]
    Parameter #2 [ <optional> int $slabid ]
    Parameter #3 [ <optional> int $limit ]
  }
}
Function [ <internal:memcache> function memcache_set_compress_threshold ] {

  - Parameters [3] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> int $threshold ]
    Parameter #2 [ <optional> float $min_savings ]
  }
  - Return [ bool ]
}
Function [ <internal:memcache> function memcache_increment ] {

  - Parameters [5] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> int $value ]
    Parameter #3 [ <optional> int $defval ]
    Parameter #4 [ <optional> int $exptime ]
  }
}
Function [ <internal:memcache> function memcache_decrement ] {

  - Parameters [5] {
    Parameter #0 [ <required> MemcachePool $memcache ]
    Parameter #1 [ <required> $key ]
    Parameter #2 [ <optional> int $value ]
    Parameter #3 [ <optional> int $defval ]
    Parameter #4 [ <optional> int $exptime ]
  }
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
    Parameter #1 [ <optional> int $delay ]
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