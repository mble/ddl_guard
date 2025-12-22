# ddl_guard

A tiny Postgres extension to disable DDL commands for non-superusers. Useful when controlling things for logical replication.

## Installation

```bash
make
make install
```

## Docker Tests

Build the test image and run the full regression suite in a container:

```bash
docker build -t ddl_guard-tests -f docker/Dockerfile .
docker run --rm ddl_guard-tests
```

To test local changes without installing PostgreSQL on the host, bind-mount the repo:

```bash
docker build -t ddl_guard-tests -f docker/Dockerfile .
docker run --rm -v "$PWD":/workspace ddl_guard-tests
```

## Usage

Add as a `shared_preload_library` in your `postgresql.conf`:

```conf
shared_preload_libraries = 'ddl_guard'
```

Then restart your PostgreSQL server.

```sql
CREATE EXTENSION ddl_guard;
ALTER SYSTEM SET ddl_guard.enabled = on;
```

Note: `ddl_guard` creates the `ddl_guard` schema; ensure it does not already exist before running `CREATE EXTENSION`.

Now, only superusers can run DDL commands.

```sql
CREATE TABLE foo (id serial);
ERROR:  Non-superusers are not allowed to execute DDL statements
HINT:  ddl_guard.enabled is set.
```

For a full list of DDL commands that are blocked, see `ddl_command_start` in https://www.postgresql.org/docs/current/event-trigger-matrix.html.

## Sentinel Mode

In some cases, instead of blocking DDL, writing a sentinel entry to an internal log table is enough. This can be enabled by setting `ddl_guard.ddl_sentinel` to `on`.

```sql
ALTER SYSTEM SET ddl_guard.ddl_sentinel = on;
```

Now, instead of blocking, a warning is emitted and a sentinel entry is written to `ddl_guard.sentinel_log` with `logged_at` and `operation`:

```sql
CREATE TABLE foo (id serial);
WARNING:  ddl_guard: ddl detected, sentinel entry written
CREATE TABLE
```

This can also be used in conjunction with `ddl_guard.lo_sentinel` to log large object operations.

```sql
ALTER SYSTEM SET ddl_guard.lo_sentinel = on;
```

Now, instead of blocking, a warning is emitted and a sentinel entry is written to `ddl_guard.sentinel_log` (readable by all users):

```sql
SELECT lo_create(0);
WARNING:  lo_guard: lobject "be_lo_create" function call, sentinel entry written
 lo_create
-----------
     51058
(1 row)
```

Sentinel logging for data control statements (role and membership changes that do not fire event triggers) can be enabled with `ddl_guard.dcl_sentinel`.

```sql
ALTER SYSTEM SET ddl_guard.dcl_sentinel = on;
```

## Configuration

The following configuration options are available:

- `ddl_guard.enabled`: Enables or disables the extension. Default is `off`.
- `ddl_guard.ddl_sentinel`: Enables "sentinel mode" for DDL. Default is `off`.
- `ddl_guard.dcl_sentinel`: Enables "sentinel mode" for data control statements. Default is `off`.
- `ddl_guard.lo_sentinel`: Enables "sentinel mode" for large objects. Default is `off`.

## License

PostgreSQL License
