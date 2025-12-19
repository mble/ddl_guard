-- ddl_guard--1.0.sql
CREATE SCHEMA IF NOT EXISTS ddl_guard;
CREATE TABLE IF NOT EXISTS ddl_guard.sentinel_log (
    logged_at timestamptz NOT NULL DEFAULT now(),
    operation text NOT NULL
);
GRANT USAGE ON SCHEMA ddl_guard TO PUBLIC;
GRANT SELECT, INSERT ON ddl_guard.sentinel_log TO PUBLIC;

CREATE OR REPLACE FUNCTION ddl_guard_check()
    RETURNS event_trigger
    SECURITY INVOKER
    SET search_path = 'pg_catalog, pg_temp'
    LANGUAGE C
    AS 'MODULE_PATHNAME', 'ddl_guard_check';

CREATE EVENT TRIGGER ddl_guard_trigger
    ON ddl_command_start
    EXECUTE FUNCTION ddl_guard_check();
