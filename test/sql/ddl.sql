SELECT current_setting('ddl_guard.enabled', true) = 'on' AS ddl_guard_enabled;
SELECT current_setting('is_superuser', true) = 'on' AS is_superuser;
CREATE TABLE foobar (id serial PRIMARY KEY);