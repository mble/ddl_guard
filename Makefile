# Makefile
MODULES = ddl_guard
EXTENSION = ddl_guard
DATA = ddl_guard--1.0.1.sql ddl_guard--1.0.2.sql ddl_guard--1.0.1--1.0.2.sql

INPUTDIR ?= test/regular
TESTS = $(wildcard $(INPUTDIR)/sql/*.sql)
REGRESS = $(patsubst $(INPUTDIR)/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=$(INPUTDIR) --load-extension=$(EXTENSION) --temp-config=test/ddl_guard.conf --use-existing

OPTFLAGS ?=
PG_CFLAGS += $(OPTFLAGS) -Werror

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
