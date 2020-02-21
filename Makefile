MODULES = pg_rational
EXTENSION = pg_rational
DATA = pg_rational--0.0.1.sql pg_rational--0.0.1--0.0.2.sql
REGRESS = pg_rational_test
PG_CPPFLAGS = -std=c99 -Wextra -Wpedantic

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
