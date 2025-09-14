/* src/crfsuite_wrapper.h */
#ifndef CRFSUITE_WRAPPER_H
#define CRFSUITE_WRAPPER_H

#include "postgres.h"
#include "crfsuite.h"

/* Error codes */
typedef enum
{
    CRF_SUCCESS = 0,
    CRF_ERROR_MEMORY = -1,
    CRF_ERROR_INVALID_MODEL = -2,
    CRF_ERROR_MODEL_LOAD = -3,
    CRF_ERROR_PREDICTION = -4,
    CRF_ERROR_DATABASE = -5
} CRFErrorCode;

/* Model structure */
typedef struct
{
    crfsuite_model_t *model;
    crfsuite_dictionary_t *labels;
    crfsuite_dictionary_t *attrs;
    char *model_name;
    char *version;
    size_t model_size;
    bool is_loaded;
} CRFModel;

/* Token structure */
typedef struct
{
    char *text;
    char *label;
    float confidence;
    int start_pos;
    int end_pos;
} Token;

/* Parse result structure */
typedef struct
{
    Token *tokens;
    int num_tokens;
    float overall_confidence;
    char *model_version;
    int processing_time_ms;
} ParseResult;

/* Feature structure for CRF input */
typedef struct
{
    char **features;
    int num_features;
    float *weights;
} TokenFeatures;

/* Function prototypes */
CRFModel *create_crf_model(void);
CRFErrorCode load_model_from_bytea(CRFModel *model, const char *model_data, size_t data_size);
CRFErrorCode predict_sequence(CRFModel *model, crfsuite_instance_t *instance,
                              int **labels, floatval_t *score);
void free_crf_model(CRFModel *model);
void free_parse_result(ParseResult *result);

/* Model management */
CRFErrorCode load_model_from_database(const char *model_name);
CRFErrorCode load_default_model(void);
CRFModel *get_active_model(void);

/* Utility functions */
char *escape_model_data(const char *data, size_t size);
size_t get_model_size(const char *model_name);

#endif /* CRFSUITE_WRAPPER_H */