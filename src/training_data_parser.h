/* src/training_data_parser.h */
#ifndef TRAINING_DATA_PARSER_H
#define TRAINING_DATA_PARSER_H

#include <stdbool.h>

/* Maximum tokens per name sequence */
#define MAX_TOKENS_PER_NAME 32

/* A single labeled token from training data */
typedef struct {
  char *text;  /* Token text */
  char *label; /* Label (e.g., "GivenName", "Surname") */
} LabeledToken;

/* A name sequence (one training example) */
typedef struct {
  LabeledToken *tokens;
  int num_tokens;
} LabeledSequence;

/* Collection of training sequences */
typedef struct {
  LabeledSequence *sequences;
  int num_sequences;
  int capacity;
} TrainingData;

/* Parse XML training file into TrainingData structure */
TrainingData *parse_training_file(const char *filename);

/* Free training data */
void free_training_data(TrainingData *data);

/* Print training data summary (for debugging) */
void print_training_summary(TrainingData *data);

#endif /* TRAINING_DATA_PARSER_H */
