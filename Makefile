EXTENSION = pg_probablepeople
MODULE_big = pg_probablepeople
DATA = pg_probablepeople--1.0.0.sql
OBJS = pg_probablepeople.o crfsuite_wrapper.o

# Link against CRFSuite library
SHLIB_LINK = -lcrfsuite
PG_LDFLAGS += -L/usr/local/lib

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
