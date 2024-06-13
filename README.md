# ddl_guard

A tiny Postgres extension to disable DDL commands for non-superusers. Useful when controlling things for logical replication.

## Installation

```bash
make
make install
```

## Usage

Add as a `shared_preload_library` in your `postgresql.conf`:

```conf
shared_preload_libraries = 'ddl_guard'
```

Then restart your PostgreSQL server.

```sql
CREATE EXTENSION ddl_guard SCHEMA pg_catalog;
ALTER SYSTEM SET ddl_guard.enabled = on;
```

Now, only superusers can run DDL commands.

```sql
CREATE TABLE foo (id serial);
ERROR:  Non-superusers are not allowed to execute DDL statements
HINT:  ddl_guard.enabled is set.
```

For a full list of DDL commands that are blocked, see `ddl_command_start` in https://www.postgresql.org/docs/current/event-trigger-matrix.html.

## Sentinel Mode

In some cases, instead of blocking DDL, dropping a sentinel file to indicate DDL has been ran is enough. This can be enabled by setting `ddl_guard.ddl_sentinel` to `on`.

```sql
ALTER SYSTEM SET ddl_guard.ddl_sentinel = on;
```

Now, instead of blocking, a warning is emitted and a sentinel file is dropped as `$PGDATA/pg_stat_tmp/ddl_guard_ddl_sentinel`:

```sql
CREATE TABLE foo (id serial);
WARNING:  ddl_guard: ddl detected, sentinel file written
CREATE TABLE
```

This can also be used in conjunction with `ddl_guard.lo_sentinel` to drop a sentinel file for large objects.

```sql
ALTER SYSTEM SET ddl_guard.lo_sentinel = on;
```

Now, instead of blocking, a warning is emitted and a sentinel file is dropped as `$PGDATA/pg_stat_tmp/ddl_guard_lo_sentinel`:

```sql
SELECT lo_create(0);
WARNING:  lo_guard: lobject write function call, sentinel file written
WARNING:  lo_guard: pg_largeobject creation detected, sentinel file written
 lo_create
-----------
     51058
(1 row)
```

## Configuration

The following configuration options are available:

- `ddl_guard.enabled`: Enables or disables the extension. Default is `off`.
- `ddl_guard.ddl_sentinel`: Enables "sentinel mode" for DDL. Default is `off`.
- `ddl_guard.lo_sentinel`: Enables "sentinel mode" for large objects. Default is `off`.

## License

PostgreSQL License
