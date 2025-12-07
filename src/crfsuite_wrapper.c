/* src/crfsuite_wrapper.c */
#include "postgres.h"
/* Postgres headers must come first */
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include "crfsuite_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global model instances */
static CRFModel *person_model = NULL;
static CRFModel *company_model = NULL;
static CRFModel *generic_model = NULL;
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
  ret = crfsuite_create_instance_from_memory(model_data, data_size,
                                             (void **)&model->model);
  if (ret != 0 || model->model == NULL) {
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

  if (ret != 0) {
    tagger->release(tagger);
    return CRF_ERROR_PREDICTION;
  }

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
 * Load model by type from file
 */
CRFErrorCode load_model_from_file(const char *filename,
                                  const char *model_type) {
  int ret;
  CRFModel **target_model;

  if (strcmp(model_type, "person") == 0) {
    target_model = &person_model;
  } else if (strcmp(model_type, "company") == 0) {
    target_model = &company_model;
  } else if (strcmp(model_type, "generic") == 0) {
    target_model = &generic_model;
  } else {
    return CRF_ERROR_INVALID_MODEL;
  }

  if (*target_model != NULL) {
    free_crf_model(*target_model);
  }

  *target_model = create_crf_model();
  if (*target_model == NULL) {
    return CRF_ERROR_MEMORY;
  }

  /* Load model from file */
  ret = crfsuite_create_instance_from_file(filename,
                                           (void **)&(*target_model)->model);
  if (ret != 0 || (*target_model)->model == NULL) {
    /* Failed to open */
    return CRF_ERROR_MODEL_LOAD;
  }

  /* Get label and attribute dictionaries */
  (*target_model)
      ->model->get_labels((*target_model)->model, &(*target_model)->labels);
  (*target_model)
      ->model->get_attrs((*target_model)->model, &(*target_model)->attrs);

  (*target_model)->is_loaded = true;
  (*target_model)->model_name = pstrdup(model_type);
  (*target_model)->version = pstrdup("1.0");

  ereport(LOG, (errmsg("Loaded CRF model from %s", filename)));

  return CRF_SUCCESS;
}

/*
 * Load default active models
 */
CRFErrorCode load_default_model(void) {
  char sharepath[MAXPGPATH];
  char model_path[MAXPGPATH];
  CRFErrorCode res_person, res_company, res_generic;

  get_share_path(my_exec_path, sharepath);

  /* Load Person Model */
  snprintf(model_path, MAXPGPATH,
           "%s/extension/person_learned_settings.crfsuite", sharepath);
  res_person = load_model_from_file(model_path, "person");

  /* Load Company Model */
  snprintf(model_path, MAXPGPATH,
           "%s/extension/company_learned_settings.crfsuite", sharepath);
  res_company = load_model_from_file(model_path, "company");

  /* Load Generic Model */
  snprintf(model_path, MAXPGPATH,
           "%s/extension/generic_learned_settings.crfsuite", sharepath);
  res_generic = load_model_from_file(model_path, "generic");

  if (res_person == CRF_SUCCESS || res_company == CRF_SUCCESS ||
      res_generic == CRF_SUCCESS)
    return CRF_SUCCESS;
  return CRF_ERROR_MODEL_LOAD;
}

/*
 * Get the currently active model
 */
CRFModel *get_active_model(const char *type) {
  if (type == NULL || strcmp(type, "person") == 0)
    return person_model;
  if (strcmp(type, "company") == 0)
    return company_model;
  if (strcmp(type, "generic") == 0)
    return generic_model;
  return NULL;
}

/*
 * Free CRF model resources
 */
void free_crf_model(CRFModel *model) {
  if (model == NULL)
    return;

  if (model->model != NULL) {
    model->model->release(model->model);
  }

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
 * Get model size by type
 */
size_t get_model_size(const char *model_type) {
  CRFModel *model;
  if (model_type == NULL)
    return 0;

  model = get_active_model(model_type);
  if (model != NULL && model->is_loaded) {
    return model->model_size;
  }
  return 0;
}
