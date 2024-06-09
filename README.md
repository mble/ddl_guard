# ddl_guard

A tiny Postgres extension to disable DDL commands for non-superusers. Useful when controlling things for logical replication.

## Installation

```bash
make
make install
```

## Usage

```sql
CREATE EXTENSION ddl_guard;
SET ddl_guard.enabled = on;
```

## License

PostgreSQL License
