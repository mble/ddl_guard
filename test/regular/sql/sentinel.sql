SELECT count(*) AS before_count FROM ddl_guard.sentinel_log;
\gset
CREATE TABLE ddl_guard_sentinel_test (id int);
INSERT INTO ddl_guard.sentinel_log (operation) VALUES ('spoof');
SELECT count(*) >= :before_count::int + 1 AS logged_after_create FROM ddl_guard.sentinel_log;
DROP TABLE ddl_guard_sentinel_test;
SELECT count(*) >= :before_count::int + 2 AS logged_after_drop FROM ddl_guard.sentinel_log;
