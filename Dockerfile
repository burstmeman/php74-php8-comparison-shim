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

RUN true
WORKDIR /ext
COPY . /ext

ENV PHP_CFLAGS="-fstack-protector-strong -fpic -fpie -O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
ENV PHP_CPPFLAGS="$PHP_CFLAGS"
ENV PHP_LDFLAGS="-Wl,-O1 -Wl,--hash-style=gnu -pie"

RUN phpize \
    && \
        CFLAGS="$PHP_CFLAGS" \
        CPPFLAGS="$PHP_CPPFLAGS" \
        LDFLAGS="$PHP_LDFLAGS" \
        ./configure \
            --enable-php74-php8-comparison-shim \
            --enable-php74-php8-comparison-shim-risky \
    && make -j$(nproc) \
    && make install

ENV TEST_PHP_ARGS="--show-diff"

CMD ["make", "test"]
