EXTENSION = pg_probablepeople
MODULE_big = pg_probablepeople
DATA = pg_probablepeople--1.0.0.sql generic_learned_settings.crfsuite person_learned_settings.crfsuite company_learned_settings.crfsuite

CRFSUITE_SRCS = $(wildcard src/crfsuite/src/*.c)
# Exclude training files that require external dependencies (lbfgs)
# Keep l2sgd and crfsuite_train.c which are self-contained
CRFSUITE_EXCLUDE = %/train_arow.c %/train_averaged_perceptron.c %/train_lbfgs.c %/train_passive_aggressive.c %/stub_train.c
CRFSUITE_OBJS = $(patsubst %.c,%.o,$(filter-out $(CRFSUITE_EXCLUDE), $(CRFSUITE_SRCS)))

OBJS = src/pg_probablepeople.o src/crfsuite_wrapper.o src/feature_extractor.o src/name_parser.o src/training_stubs.o $(CRFSUITE_OBJS)

REGRESS = test_parsing
REGRESS_OPTS = --inputdir=tests

PG_CPPFLAGS += -Isrc/crfsuite/include -Isrc/crfsuite/src
# SHLIB_LINK = -lcrfsuite # Linked statically via source inclusion
PG_LDFLAGS += -L/usr/local/lib

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
