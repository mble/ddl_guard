-- ddl_guard--1.0.sql
CREATE OR REPLACE FUNCTION ddl_guard_check()
RETURNS event_trigger
LANGUAGE plpgsql
AS $$
BEGIN
    IF (current_setting('ddl_guard.enabled', true) = 'on') AND (NOT current_setting('is_superuser', true) = 'on') THEN
        RAISE EXCEPTION 'Non-superusers are not allowed to execute DDL statements';
    END IF;
END;
$$;

CREATE EVENT TRIGGER ddl_guard_trigger
    ON ddl_command_start
    EXECUTE FUNCTION ddl_guard_check();
