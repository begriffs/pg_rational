MODULES = pg_rational
EXTENSION = pg_rational
DATA = pg_rational--0.0.1.sql
REGRESS = pg_rational_test
PG_CPPFLAGS = -ggdb

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
