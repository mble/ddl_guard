CREATE TABLE ddl_guard_dcl_test (id int);
GRANT SELECT ON ddl_guard_dcl_test TO PUBLIC;
REVOKE SELECT ON ddl_guard_dcl_test FROM PUBLIC;
DROP TABLE ddl_guard_dcl_test;
