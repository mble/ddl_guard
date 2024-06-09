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
CONTEXT:  PL/pgSQL function ddl_guard_check() line 4 at RAISE
```

For a full list of DDL commands that are blocked, see `ddl_command_start` in https://www.postgresql.org/docs/current/event-trigger-matrix.html.

## License

PostgreSQL License
