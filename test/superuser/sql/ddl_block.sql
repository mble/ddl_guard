SET ddl_guard.ddl_sentinel = off;
DROP ROLE IF EXISTS ddl_guard_regress;
CREATE ROLE ddl_guard_regress;
SET ROLE ddl_guard_regress;
CREATE TABLE ddl_guard_block_test (id int);
RESET ROLE;
DROP TABLE IF EXISTS ddl_guard_block_test;
DROP ROLE ddl_guard_regress;
