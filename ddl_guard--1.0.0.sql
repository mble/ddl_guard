-- ddl_guard--1.0.sql
CREATE OR REPLACE FUNCTION ddl_guard_check()
    RETURNS event_trigger
    SECURITY INVOKER
    SET search_path = 'pg_catalog, pg_temp'
    LANGUAGE C
    AS 'MODULE_PATHNAME', 'ddl_guard_check';

CREATE EVENT TRIGGER ddl_guard_trigger
    ON ddl_command_start
    EXECUTE FUNCTION ddl_guard_check();
