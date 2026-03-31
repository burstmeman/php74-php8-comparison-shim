# php74_php8_comparison_shim

Detects PHP 8.0 string-to-number comparison behavior changes while running on PHP 7.4.33.

## What it does

PHP 8.0 changed how loose comparisons between numbers and non-numeric strings work.
In PHP 7.4, the string was cast to a number for comparison. In PHP 8.0, the number is
cast to a string and the two strings are compared lexicographically.

This extension hooks into the PHP 7.4 VM at runtime, intercepts comparison opcodes, and
**only acts when the result would actually differ between PHP 7.4 and PHP 8.0**. Comparisons
where both versions agree are passed through without any overhead beyond the opcode handler
dispatch check.

Depending on the configured mode, the extension can:
- emit a deprecation warning (`report`)
- throw an `Error` exception (`error`)
- silently substitute the PHP 8.0 result (`simulate`)
- do both of the above (`simulate_and_report`)

Relevant PHP documentation:

- [PHP 8.0 Migration Guide](https://www.php.net/manual/en/migration80.php)
- [Loose comparisons](https://www.php.net/manual/en/types.comparisons.php)

### Behavior change examples

The table below shows cases where PHP 7.4 and PHP 8.0 **disagree** — these are the only
comparisons the extension intercepts.

| Comparison      | PHP 7.4 | PHP 8.0 | Reason                                    |
|-----------------|---------|---------|-------------------------------------------|
| `0 == "foo"`    | true    | false   | `"foo"` → 0 in 7.4; `"0" != "foo"` in 8.0 |
| `0 == ""`       | true    | false   | `""` → 0 in 7.4; `"0" != ""` in 8.0      |
| `42 == "42foo"` | true    | false   | `"42foo"` → 42 in 7.4; `"42" != "42foo"` in 8.0 |
| `0 < "foo"`     | false   | true    | `0 < 0` in 7.4; `"0" < "foo"` in 8.0     |
| `1 > "foo"`     | true    | false   | `1 > 0` in 7.4; `"1" < "foo"` in 8.0     |
| `0 <=> "foo"`   | 0       | -1      | 7.4: equal (0==0); 8.0: "0" < "foo"      |

Cases where both versions agree are **not intercepted**, even if the operands are a number
and a non-numeric string:

| Comparison      | PHP 7.4 | PHP 8.0 | Why skipped                                   |
|-----------------|---------|---------|-----------------------------------------------|
| `1 == "foo"`    | false   | false   | 7.4: `1 != 0`; 8.0: `"1" != "foo"` — same    |
| `0 <= "foo"`    | true    | true    | 7.4: `0 <= 0`; 8.0: `"0" <= "foo"` — same    |
| `0 > "foo"`     | false   | false   | 7.4: `0 > 0`; 8.0: `"0" > "foo"` — same      |
| `0 == "0"`      | true    | true    | numeric string, no change in either version   |

## Configuration

INI settings control startup behavior. For runtime control (sampling, deferred reports,
ignored locations), see [Runtime API](#runtime-api).

Enable the extension and set the mode:

```
extension=php74_php8_comparison_shim.so
php74_php8_comparison_shim.mode=report
php74_php8_comparison_shim.sampling_factor=0
```

Allowed values (set at startup only):

- `off`    - extension logic disabled
- `report` - emit deprecation warnings for comparisons whose result changes in PHP 8.0
- `error`  - throw an Error for comparisons whose result changes in PHP 8.0
- `simulate_and_report` - emit deprecation warnings and substitute the PHP 8.0 result
- `simulate` - substitute the PHP 8.0 result without reporting

Note: `error`, `simulate`, and `simulate_and_report` are only available when the extension
is built with `--enable-php74-php8-comparison-shim-risky`. Without that flag, those modes
are treated as `off`.

Note: `php74_php8_comparison_shim.mode` is `PHP_INI_SYSTEM` and cannot be changed at runtime
via `ini_set()`.

Sampling factor:

- `0` or `1` - check every comparison (no sampling)
- `N` (> 1) - check once per `N` intercepted comparisons (those where result would change)

Sampling is forced to `0` in `error`, `simulate_and_report`, and `simulate` modes.

Reporting mode:

- `php74_php8_comparison_shim.report_mode=sync` (default) - emit deprecations immediately
- `php74_php8_comparison_shim.report_mode=defer` - buffer and emit deprecations at request shutdown

Deferred reporting is designed for php-fpm: warnings are emitted after the response is sent,
so the request latency reflects only comparison work. The extension captures file/line at the
comparison site and emits one report per file:line.

Report buffer limit:

- `php74_php8_comparison_shim.report_limit=128` (default) - max number of buffered entries
- `0` - unlimited

## Runtime API

The extension provides PHP functions for runtime control. All functions are available when
the extension is loaded.

### `php74_php8_cmps_set_sampling(int $sampling_factor): bool`

Change the sampling factor during the request.

| Parameter          | Type | Description                                      |
|--------------------|------|--------------------------------------------------|
| `$sampling_factor` | int  | `0` or `1` = no sampling; `N` > 1 = sample 1/N   |

**Returns:** `true` if the factor was updated, `false` when the current mode forces
sampling off (`error`, `simulate`, `simulate_and_report`). On success, the internal
sample counter is reset.

---

### `php74_php8_cmps_flush_deferred(): bool`

Flush the deferred report buffer and emit buffered deprecations via `E_DEPRECATED`.

**Returns:** `true` if any reports were emitted, `false` if the extension is in sync
mode, the buffer is empty, or it was already flushed.

**Important:** In defer report mode (`report_mode=defer`) the extension does **not**
flush automatically. Call this explicitly (e.g. at the end of the script or inside
`register_shutdown_function()`) to emit buffered deprecations. If not called, deferred
entries are discarded at request end.

---

### `php74_php8_cmps_get_deferred_reports(): array`

Return the buffered deferred reports without flushing. In sync mode, returns an empty array.

**Returns:** List of associative arrays, each with:

| Key           | Type | Description                              |
|---------------|------|------------------------------------------|
| `filename`    | string | File where the comparison occurred    |
| `line`        | int    | Line number                             |
| `entry_count` | int    | Number of occurrences at that location |
| `operator`    | string | Operator (`==`, `!=`, `<`, `<=`, `<=>`, `case`) |
| `left_op`     | string | Left operand as string                 |
| `right_op`    | string | Right operand as string                |

---

### `php74_php8_cmps_set_ignored_locations(array $locations): void`

Suppress comparison checks at specific file:line locations. Each array element must be
a string in the form `"path_suffix:line"`, where `path_suffix` is the trailing part of
the full path (e.g. `"vendor/pkg/Class.php:105"` matches `/var/www/app/vendor/pkg/Class.php` at line 105).

| Parameter   | Type  | Description                                      |
|-------------|-------|--------------------------------------------------|
| `$locations`| array | List of `"path_suffix:line"` strings to ignore   |

**Example:**

```php
php74_php8_cmps_set_ignored_locations([
    basename(__FILE__) . ':42',
    'vendor/legacy/Helper.php:105',
]);
```

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

Debug symbols are disabled by default. To build with debug symbols, pass `CFLAGS`:

```
phpize
CFLAGS="-g -O0" ./configure --enable-php74-php8-comparison-shim
make -j$(nproc)
make install
```

To enable error or simulate modes, pass the risky flag at build time:

```
phpize
./configure --enable-php74-php8-comparison-shim --enable-php74-php8-comparison-shim-risky
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

Run the benchmark inside the container:

```
docker run --rm php74-php8-comparison-shim-test bench/run.sh
```

## Benchmark (overhead)

Build the extension first, then run:

```
chmod +x bench/run.sh
PHP_BIN=/opt/php/7.4.33/bin/php SNC_ITERATIONS=1000000 SNC_RUNS=5 bench/run.sh
```

Benchmark results (PHP 7.4.33, 1,000,000 iterations, 5 runs, aarch64 Linux):

`% diff` is computed from `avg_total_elapsed_ms` against the no-extension baseline.

| Case                                       | Avg total (ms) | Avg comparisons (ms) | Avg flush (μs) | % diff vs baseline |
|--------------------------------------------|----------------|----------------------|----------------|--------------------|
| No extension (disabled)                    | 155            | 148                  | —              | 0.0%               |
| Extension loaded: Off                      | 157            | 150                  | —              | +1.3%              |
| Extension loaded: Report                   | 853            | 846                  | —              | +450.3%            |
| Extension loaded: Report (sampling=5)      | 418            | 411                  | —              | +169.7%            |
| Extension loaded: Simulate                 | 328            | 321                  | —              | +111.6%            |
| Extension loaded: Simulate + Report        | 764            | 758                  | —              | +393.5%            |
| Extension loaded: Error                    | 899            | 893                  | —              | +480.0%            |
| Opcodes iteration overhead                 | 215            | 208                  | —              | +38.7%             |
| Report cost: all intercepted (sync)        | 1030           | 1024                 | —              | +564.5%            |
| Extension loaded: Report (defer)           | 415            | 409                  | 7              | +167.7%            |

**Notes:**
- *Off*: handlers not installed — overhead is pure module load cost (~1%).
- *Report*: overhead comes from `zend_error(E_DEPRECATED)` on every changed comparison.
- *Simulate*: cheaper than report because string result substitution avoids the error path.
- *Opcodes iteration overhead*: measures bare opcode-handler dispatch (numeric strings only, no interception).
- *Report cost: all intercepted (sync)*: isolated cost of intercepting every comparison and calling `zend_error()` inline.
- *Defer*: the comparison loop only buffers entries — no `zend_error()` in the hot path. `bench.php` calls `php74_php8_cmps_flush_deferred()` explicitly after the loop so comparison and reporting costs are measured separately. Avg flush shows the time to emit buffered `E_DEPRECATED` calls at request shutdown (~7 μs for this synthetic test with a single unique `file:line` entry).

**Production note:** The synthetic benchmark above stresses a tight loop of changed comparisons
and reflects worst-case overhead. In practice, at ~300k RPM the observed latency increase was
around **2%** — well within acceptable margins. Use the benchmark as a directional guide, not
as the deciding factor when evaluating whether to deploy the extension.

## Debugging with gdb

Start PHP with the extension loaded:

```
php -d extension=php74_php8_comparison_shim.so -d php74_php8_comparison_shim.mode=Report your_script.php
```

Then attach gdb:

```
gdb -p $(pgrep -n php)
```
