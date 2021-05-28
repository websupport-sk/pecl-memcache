ARG PHP_IMAGE=php:8.0
FROM $PHP_IMAGE

RUN docker-php-ext-configure pcntl --enable-pcntl \
    && docker-php-ext-install -j$(nproc) pcntl

RUN apt-get update && apt-get install -y \ 
	git \
	zlib1g-dev \
	memcached ;

COPY docker/host.conf /etc/host.conf

# ENV LOCAL_DEV 1
# ADD . /usr/src/pecl-memcache
COPY docker/start.sh /
CMD ["/start.sh"]
