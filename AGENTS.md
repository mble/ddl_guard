# Repository Guidelines

## Project Structure & Module Organization
- `ddl_guard.c` contains the extension implementation and hooks.
- `ddl_guard.control` and `ddl_guard--1.0.0.sql` define extension metadata and SQL objects.
- `test/regular` and `test/superuser` hold pg_regress suites with `sql/` inputs and `expected/` outputs (matching filenames).
- `test/ddl_guard.conf` provides a minimal config for regression runs.
- `bin/` and `results/` may contain build/test artifacts; avoid committing generated files unless intentional.

## Build, Test, and Development Commands
- `make` builds the shared library using PGXS.
- `make install` installs the extension into your Postgres instance (requires appropriate permissions).
- `make installcheck` runs regression tests via pg_regress using `test/regular` by default.
- `make installcheck INPUTDIR=test/superuser` runs the superuser test suite.
- `PG_CONFIG=/path/to/pg_config make` targets a specific Postgres installation when multiple versions are installed.

## Coding Style & Naming Conventions
- C code follows PostgreSQL extension conventions: tabs for indentation, K&R braces, and `ereport/errmsg` for errors.
- Keep symbols and GUC names aligned with existing patterns (`ddl_guard.*`).
- Test files use concise snake_case names (e.g., `ddl.sql`, `lobject.sql`) mirrored in `expected/`.

## Testing Guidelines
- Tests are pg_regress suites; each `sql/*.sql` file must have a matching `expected/*.out`.
- Add new tests to both `regular` and `superuser` suites when behavior differs by role.
- Run `make installcheck` locally before submitting changes that affect behavior.

## Commit & Pull Request Guidelines
- Recent commits use short, scoped prefixes like `feat:`, `tests:`, or simple descriptions (e.g., `update readme`).
- Keep commit messages imperative and focused on the change.
- PRs should describe behavior changes, include test output (or command used), and link any related issue if applicable.

## Configuration & Runtime Notes
- The extension must be loaded via `shared_preload_libraries` and created with `CREATE EXTENSION ddl_guard`.
- Key GUCs: `ddl_guard.enabled`, `ddl_guard.ddl_sentinel`, `ddl_guard.lo_sentinel`.
- Sentinel mode writes to `$PGDATA/pg_stat_tmp/` and should be accounted for in ops workflows.
