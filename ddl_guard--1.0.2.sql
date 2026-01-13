-- ddl_guard--1.0.2.sql
CREATE SCHEMA ddl_guard;
CREATE TABLE ddl_guard.sentinel_log (
    logged_at timestamptz NOT NULL DEFAULT now(),
    operation text NOT NULL
);
GRANT USAGE ON SCHEMA ddl_guard TO PUBLIC;
GRANT SELECT ON ddl_guard.sentinel_log TO PUBLIC;
