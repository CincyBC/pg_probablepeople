/* src/training_stubs.c - Stubs for unused training algorithms */
#include <crfsuite.h>

/* Stub implementations for training algorithms we're not using */
/* These are referenced by crfsuite_train.c but we only use l2sgd */

typedef void *encoder_t;
typedef void *dataset_t;
typedef void *logging_t;

void crfsuite_train_lbfgs_init(crfsuite_params_t *params) {
  /* Not implemented - we use l2sgd */
}

int crfsuite_train_lbfgs(encoder_t *gm, dataset_t *trainset, dataset_t *testset,
                         crfsuite_params_t *params, logging_t *lg,
                         floatval_t **ptr_w) {
  return 1; /* Return error - not implemented */
}

void crfsuite_train_averaged_perceptron_init(crfsuite_params_t *params) {
  /* Not implemented */
}

int crfsuite_train_averaged_perceptron(encoder_t *gm, dataset_t *trainset,
                                       dataset_t *testset,
                                       crfsuite_params_t *params, logging_t *lg,
                                       floatval_t **ptr_w) {
  return 1;
}

void crfsuite_train_passive_aggressive_init(crfsuite_params_t *params) {
  /* Not implemented */
}

int crfsuite_train_passive_aggressive(encoder_t *gm, dataset_t *trainset,
                                      dataset_t *testset,
                                      crfsuite_params_t *params, logging_t *lg,
                                      floatval_t **ptr_w) {
  return 1;
}

void crfsuite_train_arow_init(crfsuite_params_t *params) {
  /* Not implemented */
}

int crfsuite_train_arow(encoder_t *gm, dataset_t *trainset, dataset_t *testset,
                        crfsuite_params_t *params, logging_t *lg,
                        floatval_t **ptr_w) {
  return 1;
}
