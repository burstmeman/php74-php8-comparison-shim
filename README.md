# php74_php8_comparison_shim

Detects PHP 8.0 string-to-number comparison behavior changes while running on PHP 7.4.33.

## What it does

PHP 8 changed non-strict comparisons between numbers and non-numeric strings. This extension
detects those comparisons at runtime on PHP 7.4 and can either report them or throw an error.

Relevant PHP documentation:
- [PHP 8.0 Migration Guide](https://www.php.net/manual/en/migration80.php)
- [Loose comparisons](https://www.php.net/manual/en/types.comparisons.php)
- [Error handling](https://www.php.net/manual/en/language.errors.php7.php)
 
Behavior change examples:

| Comparison | PHP 7.4 | PHP 8.0 |
| --- | --- | --- |
| `0 == "0"` | true | true |
| `0 == "0.0"` | true | true |
| `0 == "foo"` | true | false |
| `0 == ""` | true | false |
| `42 == " 42"` | true | true |
| `42 == "42foo"` | true | false |

## Configuration

Enable the extension and set the mode:

```
extension=php74_php8_comparison_shim.so
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.sampling_factor=0
```

Allowed values (set at startup only):

- `off`    - extension logic disabled
- `report` - emit deprecation warnings
- `error`  - throw an Error

Note: `php74_php8_comparison_shim.mode` is `PHP_INI_SYSTEM` and cannot be changed at runtime
via `ini_set()`.

Sampling factor:
- `0` or `1` - check every comparison (no sampling)
- `N` (> 1) - check once per `N` number/string comparisons

## Install

1. Download the latest release archive from
   [GitHub Releases](https://github.com/burstmeman/php74-php8-comparison-shim/releases).
2. Extract the archive and enter the directory.
3. Build and install:

```
phpize
./configure --enable-php74-php8-comparison-shim
make -j$(nproc)
make install
```

4. Enable the extension in `php.ini`:

```
extension=php74_php8_comparison_shim.so
php74_php8_comparison_shim.mode=report
```

## Build (PHP 7.4.33)

Prerequisites:

- PHP 7.4.33 with development headers (`phpize`, `php-config`)
- Build tools (`make`, `autoconf`, compiler toolchain)

From the extension directory:

```
phpize
./configure --enable-php74-php8-comparison-shim
make -j$(nproc)
```

Install the module to your PHP extension dir:

```
make install
```

Optional debug build:

```
CFLAGS="-g -O0" ./configure --enable-php74-php8-comparison-shim
```

Use this when you want debug symbols and no optimizations for easier gdb debugging.

## Use with PHP

Find where `make install` placed the module:

```
php-config --extension-dir
```

Then enable it via `php.ini`:

```
extension=php74_php8_comparison_shim.so
php74_php8_comparison_shim.mode=report
```

Or enable it for a single run:

```
php -d extension=php74_php8_comparison_shim.so \
    -d php74_php8_comparison_shim.mode=report \
    your_script.php
```

## Run tests

Build the extension first, then run:

```
make test
```

If multiple PHP versions are installed, point to the PHP 7.4 binary:

```
make test TEST_PHP_EXECUTABLE=/usr/bin/php7.4
```

To run a single test:

```
TESTS=tests/002-report.phpt make test
```

## Docker (Ubuntu)

Prepare the container (build image):

```
docker build -t php74-php8-comparison-shim-test .
```

Run the PHPT suite inside the container:

```
docker run --rm php74-php8-comparison-shim-test
```

Run a single PHPT:

```
docker run --rm php74-php8-comparison-shim-test bash -lc "TESTS=tests/002-report.phpt make test"
```

## Benchmark (overhead)

Build the extension first, then run:

```
chmod +x bench/run.sh
PHP_BIN=/opt/php/7.4.33/bin/php SNC_ITERATIONS=1000000 SNC_RUNS=5 bench/run.sh
```

Benchmark results (PHP 7.4.33, 1,000,000 iterations, 5 runs):

| Case | Avg elapsed (ms) |
| --- | --- |
| No extension (disabled) | 63 |
| Extension loaded: Off | 58 |
| Extension loaded: Report | 648 |
| Extension loaded: Report (sampling=5) | 227 |
| Extension loaded: Error | 353 |
| Opcode overhead (no report) | 90 |
| Deprecated cost (with report) | 574 |

## Debugging with gdb

Start PHP with the extension loaded:

```
php -d extension=php74_php8_comparison_shim.so -d php74_php8_comparison_shim.mode=Report your_script.php
```

Then attach gdb:

```
gdb -p $(pgrep -n php)
```
