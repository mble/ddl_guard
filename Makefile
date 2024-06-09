# Makefile
MODULES = ddl_guard
EXTENSION = ddl_guard
DATA = ddl_guard--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)