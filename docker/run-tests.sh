#!/usr/bin/env bash

set -euo pipefail

PG_MAJOR="${PG_MAJOR:-16}"
PG_BIN="/usr/lib/postgresql/${PG_MAJOR}/bin"
WORKSPACE="${WORKSPACE:-/workspace}"

export PATH="${PG_BIN}:${PATH}"
export PGDATA="${PGDATA:-/var/lib/postgresql/data}"
export PGHOST="${PGHOST:-/tmp}"
export PGPORT="${PGPORT:-5432}"

install -d -o postgres -g postgres "${PGDATA}"

make -C "${WORKSPACE}" clean
PG_CFLAGS="-DUSE_ASSERT_CHECKING -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare" \
	make -C "${WORKSPACE}"
make -C "${WORKSPACE}" install

if [ ! -s "${PGDATA}/PG_VERSION" ]; then
	su - postgres -c "env PATH=${PATH} ${PG_BIN}/initdb --auth=trust -D ${PGDATA}" >/tmp/initdb.log
fi

sed -i -e "/^[[:space:]]*#\\?shared_preload_libraries[[:space:]]*=/d" \
	"${PGDATA}/postgresql.conf"
echo "shared_preload_libraries = 'ddl_guard'" >> "${PGDATA}/postgresql.conf"

su - postgres -c "env PATH=${PATH} PGDATA=${PGDATA} ${PG_BIN}/pg_ctl -D ${PGDATA} -o \"-c listen_addresses='' -c unix_socket_directories=${PGHOST} -c ddl_guard.enabled=on -c ddl_guard.ddl_sentinel=on -c ddl_guard.lo_sentinel=on\" -l /tmp/postgres.log -w start"

cleanup() {
	su - postgres -c "env PATH=${PATH} PGDATA=${PGDATA} ${PG_BIN}/pg_ctl -D ${PGDATA} -m fast stop" >/tmp/pg_stop.log
}
trap cleanup EXIT

psql -v ON_ERROR_STOP=1 -U postgres -d postgres -c "DROP ROLE IF EXISTS testuser"
psql -v ON_ERROR_STOP=1 -U postgres -d postgres -c "CREATE ROLE testuser LOGIN"
psql -v ON_ERROR_STOP=1 -U postgres -d postgres -c "DROP DATABASE IF EXISTS contrib_regression"
psql -v ON_ERROR_STOP=1 -U postgres -d postgres -c "CREATE DATABASE contrib_regression"
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c "GRANT ALL ON SCHEMA public TO testuser"
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c 'CREATE EXTENSION ddl_guard SCHEMA pg_catalog;' > /dev/null
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c 'ALTER SYSTEM SET ddl_guard.enabled = on;' > /dev/null
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c 'ALTER SYSTEM SET ddl_guard.ddl_sentinel = on;' > /dev/null
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c 'ALTER SYSTEM SET ddl_guard.lo_sentinel = on;' > /dev/null
psql -v ON_ERROR_STOP=1 -U postgres -d contrib_regression -c 'SELECT pg_reload_conf();' > /dev/null

PGUSER=testuser INPUTDIR=test/regular make -C "${WORKSPACE}" installcheck
PGUSER=postgres INPUTDIR=test/superuser make -C "${WORKSPACE}" installcheck
