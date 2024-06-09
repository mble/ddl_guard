SELECT current_setting('ddl_guard.enabled', true) = 'on' AS ddl_guard_enabled;
SELECT current_setting('is_superuser', true) = 'on' AS is_superuser;
CREATE TABLE IF NOT EXISTS foobar (id serial PRIMARY KEY);
CREATE OR REPLACE FUNCTION foobar_trigger() RETURNS TRIGGER AS $$
BEGIN
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
DROP EXTENSION ddl_guard;
SET ddl_guard.enabled TO off;
