FROM php:7.3-stretch

ENV CFLAGS="-fstack-protector-strong -fpic -fpie -O2"
ENV CPPFLAGS="$PHP_CFLAGS"
ENV LDFLAGS="-Wl,-O1 -Wl,--hash-style=both -pie"

ENV PECL_MEMCACHE_URL_GIT="https://github.com/websupport-sk/pecl-memcache.git"

RUN apt-get update && apt-get install -y \ 
	git \
	zlib1g-dev \
	memcached ; 

RUN mkdir -p /var/run/memcached
RUN chown memcache:memcache /var/run/memcached

COPY docker/host.conf /etc/host.conf

RUN set -eux;\
	cd /usr/src; \
	\
	git clone "$PECL_MEMCACHE_URL_GIT"; \
	\
	cd pecl-memcache; \
	phpize ; \
	./configure ; \
	make -j2; 

RUN echo "" 

COPY docker/start.sh /
CMD ["/start.sh"]
