ARG PHP_IMAGE=php:7.3-stretch
FROM $PHP_IMAGE

RUN apt-get update && apt-get install -y \ 
	git \
	zlib1g-dev \
	memcached ; 

RUN mkdir -p /var/run/memcached
RUN chown memcache:memcache /var/run/memcached

COPY docker/host.conf /etc/host.conf

COPY docker/start.sh /
CMD ["/start.sh"]
