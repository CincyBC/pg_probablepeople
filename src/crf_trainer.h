/* src/crf_trainer.h */
#ifndef CRF_TRAINER_H
#define CRF_TRAINER_H

#include "training_data_parser.h"
#include <crfsuite.h>

/* Training configuration */
typedef struct {
  float c2;           /* L2 regularization coefficient (default: 1.0) */
  int max_iterations; /* Maximum iterations (default: 100) */
  float epsilon;      /* Convergence threshold (default: 0.0001) */
} TrainingConfig;

/* Initialize default training config */
void init_training_config(TrainingConfig *config);

/* Train a CRF model from training data
 * Returns 0 on success, non-zero on error
 */
int train_crf_model(TrainingData *data, const char *output_file,
                    TrainingConfig *config);

/* Train a combined (generic) model from multiple training files */
int train_generic_model(const char *person_file, const char *company_file,
                        const char *output_file, TrainingConfig *config);

#endif /* CRF_TRAINER_H */
