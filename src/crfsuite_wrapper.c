/* src/crfsuite_wrapper.c */
#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"

#include "crfsuite_wrapper.h"
#include <string.h>
#include <stdlib.h>

/* Global model instance */
static CRFModel *global_model = NULL;
extern MemoryContext crf_memory_context;

/*
 * Create a new CRF model structure
 */
CRFModel* create_crf_model(void) {
    MemoryContext oldcontext;
    CRFModel *model;
    
    oldcontext = MemoryContextSwitchTo(crf_memory_context);
    
    model = (CRFModel *) palloc0(sizeof(CRFModel));
    if (model == NULL) {
        MemoryContextSwitchTo(oldcontext);
        return NULL;
    }
    
    model->model = NULL;
    model->labels = NULL;
    model->attrs = NULL;
    model->model_name = NULL;
    model->version = NULL;
    model->model_size = 0;
    model->is_loaded = false;
    
    MemoryContextSwitchTo(oldcontext);
    return model;
}

/*
 * Load CRF model from binary data (BYTEA)
 */
CRFErrorCode load_model_from_bytea(CRFModel *model, const char *model_data, size_t data_size) {
    int ret;
    FILE *temp_file;
    char temp_filename[256];
    
    if (model == NULL || model_data == NULL || data_size == 0) {
        return CRF_ERROR_INVALID_MODEL;
    }
    
    /* Create temporary file for model data */
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/crfmodel_%p.tmp", (void*)model);
    temp_file = fopen(temp_filename, "wb");
    if (temp_file == NULL) {
        ereport(ERROR, (errcode(ERRCODE_IO_ERROR),
                       errmsg("Failed to create temporary file for CRF model")));
        return CRF_ERROR_MODEL_LOAD;
    }
    
    /* Write model data to temporary file */
    if (fwrite(model_data, 1, data_size, temp_file) != data_size) {
        fclose(temp_file);
        unlink(temp_filename);
        return CRF_ERROR_MODEL_LOAD;
    }
    fclose(temp_file);
    
    /* Create CRFSuite model object */
    ret = crfsuite_model_new(&model->model);
    if (ret != CRFSUITE_SUCCESS) {
        unlink(temp_filename);
        return CRF_ERROR_MEMORY;
    }
    
    /* Load model from temporary file */
    ret = crfsuite_model_open(model->model, temp_filename);
    if (ret != CRFSUITE_SUCCESS) {
        crfsuite_model_delete(model->model);
        model->model = NULL;
        unlink(temp_filename);
        return CRF_ERROR_MODEL_LOAD;
    }
    
    /* Get label and attribute dictionaries */
    crfsuite_model_get_labels(model->model, &model->labels);
    crfsuite_model_get_attrs(model->model, &model->attrs);
    
    model->model_size = data_size;
    model->is_loaded = true;
    
    /* Cleanup temporary file */
    unlink(temp_filename);
    
    return CRF_SUCCESS;
}

/*
 * Predict label sequence using CRF model
 */
CRFErrorCode predict_sequence(CRFModel *model, crfsuite_instance_t *instance, 
                             int **labels, floatval_t *score) {
    int ret;
    crfsuite_tagger_t *tagger = NULL;
    
    if (model == NULL || !model->is_loaded || instance == NULL) {
        return CRF_ERROR_INVALID_MODEL;
    }
    
    /* Create tagger */
    ret = crfsuite_model_get_tagger(model->model, &tagger);
    if (ret != CRFSUITE_SUCCESS || tagger == NULL) {
        return CRF_ERROR_PREDICTION;
    }
    
    /* Set instance for tagging */
    ret = crfsuite_tagger_set(tagger, instance);
    if (ret != CRFSUITE_SUCCESS) {
        crfsuite_tagger_delete(tagger);
        return CRF_ERROR_PREDICTION;
    }
    
    /* Allocate memory for label sequence */
    int num_items = crfsuite_instance_length(instance);
    *labels = (int *) palloc(num_items * sizeof(int));
    if (*labels == NULL) {
        crfsuite_tagger_delete(tagger);
        return CRF_ERROR_MEMORY;
    }
    
    /* Perform Viterbi decoding */
    ret = crfsuite_tagger_viterbi(tagger, *labels, score);
    if (ret != CRFSUITE_SUCCESS) {
        pfree(*labels);
        *labels = NULL;
        crfsuite_tagger_delete(tagger);
        return CRF_ERROR_PREDICTION;
    }
    
    crfsuite_tagger_delete(tagger);
    return CRF_SUCCESS;
}

/*
 * Load model from database by name
 */
CRFErrorCode load_model_from_database(const char *model_name) {
    int ret;
    SPITupleTable *tuptable;
    TupleDesc tupdesc;
    char query[512];
    Datum model_data_datum;
    bool isnull;
    bytea *model_bytea;
    char *model_data;
    size_t data_size;
    
    if (SPI_connect() != SPI_OK_CONNECT) {
        ereport(ERROR, (errcode(ERRCODE_CONNECTION_FAILURE),
                       errmsg("Failed to connect to SPI")));
        return CRF_ERROR_DATABASE;
    }
    
    /* Query for active model or specific model */
    if (model_name == NULL) {
        snprintf(query, sizeof(query),
                "SELECT model_data, name, version FROM crfname_ner.crf_models "
                "WHERE is_active = TRUE LIMIT 1");
    } else {
        snprintf(query, sizeof(query),
                "SELECT model_data, name, version FROM crfname_ner.crf_models "
                "WHERE name = '%s' LIMIT 1", model_name);
    }
    
    ret = SPI_execute(query, true, 1);
    if (ret != SPI_OK_SELECT) {
        SPI_finish();
        ereport(ERROR, (errcode(ERRCODE_SQL_STATEMENT_NOT_YET_COMPLETE),
                       errmsg("Failed to execute model query")));
        return CRF_ERROR_DATABASE;
    }
    
    if (SPI_processed == 0) {
        SPI_finish();
        ereport(WARNING, (errmsg("No CRF model found: %s", 
                                model_name ? model_name : "active model")));
        return CRF_ERROR_MODEL_LOAD;
    }
    
    tuptable = SPI_tuptable;
    tupdesc = tuptable->tupdesc;
    
    /* Extract model data */
    model_data_datum = SPI_getbinval(tuptable->vals[0], tupdesc, 1, &isnull);
    if (isnull) {
        SPI_finish();
        return CRF_ERROR_INVALID_MODEL;
    }
    
    model_bytea = DatumGetByteaP(model_data_datum);
    data_size = VARSIZE(model_bytea) - VARHDRSZ;
    model_data = VARDATA(model_bytea);
    
    /* Free existing model if any */
    if (global_model != NULL) {
        free_crf_model(global_model);
    }
    
    /* Create new model */
    global_model = create_crf_model();
    if (global_model == NULL) {
        SPI_finish();
        return CRF_ERROR_MEMORY;
    }
    
    /* Load model data */
    CRFErrorCode load_result = load_model_from_bytea(global_model, model_data, data_size);
    if (load_result != CRF_SUCCESS) {
        free_crf_model(global_model);
        global_model = NULL;
        SPI_finish();
        return load_result;
    }
    
    /* Set model metadata */
    if (model_name) {
        global_model->model_name = pstrdup(model_name);
    } else {
        char *db_name = SPI_getvalue(tuptable->vals[0], tupdesc, 2);
        global_model->model_name = pstrdup(db_name);
    }
    
    char *db_version = SPI_getvalue(tuptable->vals[0], tupdesc, 3);
    global_model->version = pstrdup(db_version);
    
    SPI_finish();
    
    ereport(LOG, (errmsg("Successfully loaded CRF model: %s (version %s)", 
                        global_model->model_name, global_model->version)));
    
    return CRF_SUCCESS;
}

/*
 * Load default active model
 */
CRFErrorCode load_default_model(void) {
    return load_model_from_database(NULL);
}

/*
 * Get the currently active model
 */
CRFModel* get_active_model(void) {
    return global_model;
}

/*
 * Free CRF model resources
 */
void free_crf_model(CRFModel *model) {
    if (model == NULL) return;
    
    if (model->model != NULL) {
        crfsuite_model_delete(model->model);
    }
    
    if (model->labels != NULL) {
        crfsuite_dictionary_delete(model->labels);
    }
    
    if (model->attrs != NULL) {
        crfsuite_dictionary_delete(model->attrs);
    }
    
    if (model->model_name != NULL) {
        pfree(model->model_name);
    }
    
    if (model->version != NULL) {
        pfree(model->version);
    }
    
    pfree(model);
}

/*
 * Free parse result
 */
void free_parse_result(ParseResult *result) {
    if (result == NULL) return;
    
    if (result->tokens != NULL) {
        for (int i = 0; i < result->num_tokens; i++) {
            if (result->tokens[i].text != NULL) {
                pfree(result->tokens[i].text);
            }
            if (result->tokens[i].label != NULL) {
                pfree(result->tokens[i].label);
            }
        }
        pfree(result->tokens);
    }
    
    if (result->model_version != NULL) {
        pfree(result->model_version);
    }
    
    pfree(result);
}

/*
 * Get model size by name
 */
size_t get_model_size(const char *model_name) {
    if (global_model != NULL && 
        global_model->model_name != NULL && 
        strcmp(global_model->model_name, model_name) == 0) {
        return global_model->model_size;
    }
    return 0;
}
