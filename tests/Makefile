CC=gcc
CFLAGS=-I. -I../src -Wall -Wextra -g -Wno-unused-parameter
LDFLAGS=-lcrfsuite

SRCS_BASE = crfsuite_wrapper.c feature_extractor.c model_loader.c name_parser.c
SRCS = $(addprefix ../src/, $(SRCS_BASE))
MOCK_SRCS = crfsuite.c postgres.c

# test_feature_extractor doesn't need name_parser.c or crfsuite_wrapper.c
TEST_FE_SRCS = ../src/feature_extractor.c test_feature_extractor.c $(MOCK_SRCS)

# test_name_parser needs most of the src files
TEST_NP_SRCS = ../src/name_parser.c ../src/crfsuite_wrapper.c ../src/feature_extractor.c test_name_parser.c $(MOCK_SRCS)


all: test_feature_extractor test_name_parser

test_feature_extractor:
	$(CC) $(CFLAGS) -o $@ $(TEST_FE_SRCS) $(LDFLAGS)

test_name_parser:
	$(CC) $(CFLAGS) -o $@ $(TEST_NP_SRCS) $(LDFLAGS)

run_tests: all
	./test_feature_extractor
	./test_name_parser

clean:
	rm -f test_feature_extractor test_name_parser *.o
