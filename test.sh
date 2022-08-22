#!/bin/bash

set -eo pipefail

make clean && make && make install

PGDATA=`mktemp -d -t tfdw-XXXXXXXXXXX`

trap "PGDATA=\"$PGDATA\" pg_ctl stop >/dev/null || true; rm -rf \"$PGDATA\"" EXIT

PGDATA="$PGDATA" pg_ctl initdb > /dev/null
PGDATA="$PGDATA" pg_ctl start
psql postgres -f test.sql