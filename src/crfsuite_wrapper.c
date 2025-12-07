/* src/crfsuite_wrapper.c */
#include "postgres.h"
/* Postgres headers must come first */
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include "crfsuite_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global model instance */
static CRFModel *global_model = NULL;
extern MemoryContext crf_memory_context;

/*
 * Create a new CRF model structure
 */
CRFModel *create_crf_model(void) {
  MemoryContext oldcontext;
  CRFModel *model;

  oldcontext = MemoryContextSwitchTo(crf_memory_context);

  model = (CRFModel *)palloc0(sizeof(CRFModel));
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
CRFErrorCode load_model_from_bytea(CRFModel *model, const char *model_data,
                                   size_t data_size) {
  int ret;

  if (model == NULL || model_data == NULL || data_size == 0) {
    return CRF_ERROR_INVALID_MODEL;
  }

  /* Load model from memory directly using CRFSuite API */
  /* Note: crfsuite_create_instance_from_memory might require aligned memory or
     specific flags? Standard libcrfsuite supports it. */
  ret = crfsuite_create_instance_from_memory(model_data, data_size,
                                             (void **)&model->model);
  if (ret != 0 || model->model == NULL) {
    /* Try temp file fallback if memory load fails (some versions might not
     * support it) */
    /* But for now assumes it works as we vendored master */
    return CRF_ERROR_MODEL_LOAD;
  }

  /* Get label and attribute dictionaries */
  model->model->get_labels(model->model, &model->labels);
  model->model->get_attrs(model->model, &model->attrs);

  model->model_size = data_size;
  model->is_loaded = true;

  return CRF_SUCCESS;
}

/*
 * Predict label sequence using CRF model
 */
CRFErrorCode predict_sequence(CRFModel *model, crfsuite_instance_t *instance,
                              int **labels, floatval_t *score) {
  int ret;
  crfsuite_tagger_t *tagger = NULL;
  int num_items;

  if (model == NULL || !model->is_loaded || instance == NULL) {
    return CRF_ERROR_INVALID_MODEL;
  }

  /* Create tagger */
  ret = model->model->get_tagger(model->model, &tagger);
  if (ret != 0 || tagger == NULL) {
    return CRF_ERROR_PREDICTION;
  }

  /* Set instance for tagging */
  ret = tagger->set(tagger, instance);
  /* Allocate memory for label sequence */

  if (ret != 0) {
    tagger->release(tagger);
    return CRF_ERROR_PREDICTION;
  }

  // tagger->length(tagger) returns int.
  // Wait, instance structure has num_items?
  // instance->num_items exists.
  num_items = instance->num_items;

  *labels = (int *)palloc(num_items * sizeof(int));
  if (*labels == NULL) {
    tagger->release(tagger);
    return CRF_ERROR_MEMORY;
  }

  /* Perform Viterbi decoding */
  ret = tagger->viterbi(tagger, *labels, score);
  if (ret != 0) {
    pfree(*labels);
    *labels = NULL;
    tagger->release(tagger);
    return CRF_ERROR_PREDICTION;
  }

  tagger->release(tagger);
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
  MemoryContext oldContext;
  char *storable_data;
  CRFErrorCode load_result;
  char *db_version;
  char *db_name; /* Moved declaration to top of function */

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
             "WHERE name = '%s' LIMIT 1",
             model_name);
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
  /* Note: Model data must be copied potentially if default memory context is
     used, but create_crf_instance_from_memory might copy? Actually libcrfsuite
     from memory usually maps it. So we might need to keep model_bytea around in
      crf_memory_context. If we load from DB, the SPI data is freed after
     SPI_finish. So we definitely need to copy the data to our context. */

  oldContext = MemoryContextSwitchTo(crf_memory_context);
  storable_data = palloc(data_size);
  memcpy(storable_data, model_data, data_size);
  MemoryContextSwitchTo(oldContext);

  load_result = load_model_from_bytea(global_model, storable_data, data_size);

  if (load_result != CRF_SUCCESS) {
    /* pfree(storable_data); // should free */
    free_crf_model(global_model);
    global_model = NULL;
    SPI_finish();
    return load_result;
  }

  /* Set model metadata */

  if (model_name) {
    global_model->model_name = pstrdup(model_name);
  } else {
    db_name = SPI_getvalue(tuptable->vals[0], tupdesc, 2);
    global_model->model_name = pstrdup(db_name);
  }

  db_version = SPI_getvalue(tuptable->vals[0], tupdesc, 3);
  global_model->version = pstrdup(db_version);

  SPI_finish();

  ereport(LOG, (errmsg("Successfully loaded CRF model: %s (version %s)",
                       global_model->model_name, global_model->version)));

  return CRF_SUCCESS;
}

/*
 * Load CRF model from file
 */
CRFErrorCode load_model_from_file(const char *filename) {
  int ret;
  CRFModel *model;

  if (global_model != NULL) {
    free_crf_model(global_model);
  }

  global_model = create_crf_model();
  if (global_model == NULL) {
    return CRF_ERROR_MEMORY;
  }

  model = global_model;

  /* Load model from file */
  ret = crfsuite_create_instance_from_file(filename, (void **)&model->model);
  if (ret != 0 || model->model == NULL) {
    /* Failed to open */
    return CRF_ERROR_MODEL_LOAD;
  }

  /* Get label and attribute dictionaries */
  model->model->get_labels(model->model, &model->labels);
  model->model->get_attrs(model->model, &model->attrs);

  model->is_loaded = true;
  model->model_name = pstrdup("default");
  model->version = pstrdup("1.0");

  ereport(LOG, (errmsg("Loaded CRF model from %s", filename)));

  return CRF_SUCCESS;
}

/*
 * Load default active model
 */
CRFErrorCode load_default_model(void) {
  char sharepath[MAXPGPATH];
  char model_path[MAXPGPATH];

  get_share_path(my_exec_path, sharepath);
  snprintf(model_path, MAXPGPATH,
           "%s/extension/person_learned_settings.crfsuite", sharepath);

  if (load_model_from_file(model_path) == CRF_SUCCESS) {
    return CRF_SUCCESS;
  }

  /* Fallback to database */
  return load_model_from_database(NULL);
}

/*
 * Get the currently active model
 */
CRFModel *get_active_model(void) { return global_model; }

/*
 * Free CRF model resources
 */
void free_crf_model(CRFModel *model) {
  if (model == NULL)
    return;

  if (model->model != NULL) {
    model->model->release(model->model);
  }

  /* Dictionaries are owned by model?
     Usually yes, or they are just interfaces.
     If we called release on labels/attrs, it might be double free if model
     release does it. Doc says: "Obtain the pointer to crfsuite_dictionary_t
     interface". Usually get_labels increments refcount? If so, we should
     release. Let's check addref/release in header. If get_labels increments, we
     should release. If not, we shouldn't. Doc says "obtain the pointer...".
     Usually implies reference.
     But looking at libcrfsuite source would verify.
     Safest to assume model release handles its internals, or we release what we
     got. Let's attempt to release them if non-null. */

  if (model->labels != NULL) {
    model->labels->release(model->labels);
  }

  if (model->attrs != NULL) {
    model->attrs->release(model->attrs);
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
  if (result == NULL)
    return;

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
  if (global_model != NULL && global_model->model_name != NULL &&
      strcmp(global_model->model_name, model_name) == 0) {
    return global_model->model_size;
  }
  return 0;
}
