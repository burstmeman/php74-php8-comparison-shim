FROM composer:2 AS composer

FROM php:7.4-cli

ENV DEBIAN_FRONTEND=noninteractive

COPY --from=composer /usr/bin/composer /usr/local/bin/composer

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        autoconf \
        pkg-config \
        re2c \
        unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ext
COPY . /ext

RUN phpize \
    && CFLAGS="-g -O0" ./configure --enable-php74-php8-comparison-shim \
    && make -j$(nproc) \
    && make install

CMD ["make", "test"]
