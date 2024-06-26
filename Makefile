# Makefile
MODULES = ddl_guard
EXTENSION = ddl_guard
DATA = ddl_guard--1.0.0.sql

INPUTDIR ?= test/regular
TESTS = $(wildcard $(INPUTDIR)/sql/*.sql)
REGRESS = $(patsubst $(INPUTDIR)/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=$(INPUTDIR) --load-extension=$(EXTENSION) --temp-config=test/ddl_guard.conf --use-existing

OPTFLAGS = -march=native
ifeq ($(shell uname -s), Darwin)
	ifeq ($(shell uname -p), arm)
		# no difference with -march=armv8.5-a
		OPTFLAGS =
	endif
endif

PG_CFLAGS += $(OPTFLAGS) -Werror -ftree-vectorize -fassociative-math -fno-signed-zeros -fno-trapping-math

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
