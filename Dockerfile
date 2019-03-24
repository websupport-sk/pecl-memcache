ARG PHP_IMAGE=php:7.3-stretch
FROM $PHP_IMAGE

RUN apt-get update && apt-get install -y \ 
	git \
	zlib1g-dev \
	memcached ; 

COPY docker/host.conf /etc/host.conf

COPY docker/start.sh /
CMD ["/start.sh"]
