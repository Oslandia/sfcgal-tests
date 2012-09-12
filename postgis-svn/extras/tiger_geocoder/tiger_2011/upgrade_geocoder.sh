#!/bin/bash
# $Id: upgrade_geocoder.sh 9691 2012-04-29 15:25:55Z robe $
export PGPORT=5432
export PGHOST=localhost
export PGUSER=postgres
export PGPASSWORD=yourpasswordhere
THEDB=geocoder
PSQL_CMD=/usr/bin/psql
PGCONTRIB=/usr/share/postgresql/contrib
${PSQL_CMD} -d "${THEDB}" -f "upgrade_geocode.sql"
${PSQL_CMD} -d "${THEDB}" -f "tiger_loader_2011.sql"