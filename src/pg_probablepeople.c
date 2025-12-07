/* src/pg_probablepeople.c */
#include "postgres.h"
/* Postgres headers must come first */
#include "access/htup_details.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

#include "crfsuite_wrapper.h"
#include "name_parser.h"

PG_MODULE_MAGIC;

MemoryContext crf_memory_context = NULL;

void _PG_init(void);

void _PG_init(void) {
  /* Initialize memory context for long-lived model data */
  crf_memory_context = AllocSetContextCreate(
      TopMemoryContext, "CRF Model Context", ALLOCSET_DEFAULT_SIZES);

  /* Load default model on startup */
  if (load_default_model() != CRF_SUCCESS) {
    ereport(WARNING,
            (errmsg("Failed to load default CRF model for pg_probablepeople")));
  }
}

/* User context for SRF */
typedef struct {
  ParseResult *parsed;
  int current_idx;
  CRFModel *model;
} NameParserContext;

PG_FUNCTION_INFO_V1(parse_name_crf);
Datum parse_name_crf(PG_FUNCTION_ARGS) {
  FuncCallContext *funcctx;
  NameParserContext *userctx;

  if (SRF_IS_FIRSTCALL()) {
    MemoryContext oldcontext;
    text *input_text;
    char *input_str;
    CRFModel *model;
    ParseResult *result;
    TupleDesc tupdesc;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    /* Get arguments */
    if (PG_ARGISNULL(0)) {
      MemoryContextSwitchTo(oldcontext);
      SRF_RETURN_DONE(funcctx);
    }
    input_text = PG_GETARG_TEXT_PP(0);
    input_str = text_to_cstring(input_text);

    /* Get the generic model - directly use it for all name parsing */
    model = get_active_model("generic");
    if (model == NULL || !model->is_loaded) {
      if (load_default_model() != CRF_SUCCESS) {
        pfree(input_str);
        MemoryContextSwitchTo(oldcontext);
        ereport(ERROR, (errmsg("CRF model is not loaded")));
      }
      model = get_active_model("generic");
    }

    /* Fallback to person model if generic is still not available */
    if (model == NULL || !model->is_loaded) {
      model = get_active_model("person");
    }

    /* Parse name using specific model */
    /* Note: parse_name_string allocates result in current context
     * (multi_call_ctx) */
    result = parse_name_string(input_str, model);
    pfree(input_str);

    if (result != NULL) {
      userctx = (NameParserContext *)palloc(sizeof(NameParserContext));
      userctx->parsed = result;
      userctx->current_idx = 0;
      userctx->model = model;
      funcctx->user_fctx = userctx;
      funcctx->max_calls = result->num_tokens;

      /* Prepare tuple description for (token text, label text) */
      if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
        MemoryContextSwitchTo(oldcontext);
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                        errmsg("function returning record called in context "
                               "that cannot accept type record")));
      }

      funcctx->tuple_desc = BlessTupleDesc(tupdesc);
    } else {
      funcctx->user_fctx = NULL;
      funcctx->max_calls = 0;
    }

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  userctx = (NameParserContext *)funcctx->user_fctx;

  if (userctx && userctx->parsed &&
      userctx->current_idx < userctx->parsed->num_tokens) {
    Datum values[2];
    bool nulls[2] = {false, false};
    HeapTuple tuple;
    Token *token = &userctx->parsed->tokens[userctx->current_idx];

    values[0] = CStringGetTextDatum(token->text);
    values[1] = CStringGetTextDatum(token->label);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    userctx->current_idx++;

    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  } else {
    /* Cleanup handled by memory context */
    SRF_RETURN_DONE(funcctx);
  }
}

PG_FUNCTION_INFO_V1(tag_name_crf);
Datum tag_name_crf(PG_FUNCTION_ARGS) {
  text *input_text;
  char *input_str;
  CRFModel *model;
  ParseResult *result;
  JsonbValue *jbv;
  Jsonb *jb;

  if (PG_ARGISNULL(0))
    PG_RETURN_NULL();

  input_text = PG_GETARG_TEXT_PP(0);
  input_str = text_to_cstring(input_text);

  /* Get the generic model - directly use it for all name parsing */
  model = get_active_model("generic");
  if (model == NULL || !model->is_loaded) {
    if (load_default_model() != CRF_SUCCESS) {
      pfree(input_str);
      ereport(ERROR, (errmsg("CRF model is not loaded")));
    }
    model = get_active_model("generic");
  }

  /* Fallback to person model if generic is still not available */
  if (model == NULL || !model->is_loaded) {
    model = get_active_model("person");
  }

  result = parse_name_string(input_str, model);
  pfree(input_str);

  if (result == NULL)
    PG_RETURN_NULL();

  jbv = parse_result_to_jsonb(result);

  if (jbv == NULL)
    PG_RETURN_NULL();

  jb = JsonbValueToJsonb(jbv);

  PG_RETURN_JSONB_P(jb);
}

PG_FUNCTION_INFO_V1(parse_name_cols);
Datum parse_name_cols(PG_FUNCTION_ARGS) {
  text *input_text;
  char *input_str;
  CRFModel *model;
  ParseResult *result;
  ParsedNameCols *cols;
  TupleDesc tupdesc;
  Datum values[10];
  bool nulls[10];
  HeapTuple tuple;
  int i;

  if (PG_ARGISNULL(0))
    PG_RETURN_NULL();

  input_text = PG_GETARG_TEXT_PP(0);
  input_str = text_to_cstring(input_text);

  /* Get the generic model - directly use it for all name parsing */
  model = get_active_model("generic");
  if (model == NULL || !model->is_loaded) {
    if (load_default_model() != CRF_SUCCESS) {
      pfree(input_str);
      ereport(ERROR, (errmsg("CRF model is not loaded")));
    }
    model = get_active_model("generic");
  }

  /* Fallback to person model if generic is still not available */
  if (model == NULL || !model->is_loaded) {
    model = get_active_model("person");
  }

  result = parse_name_string(input_str, model);
  pfree(input_str);

  if (result == NULL)
    PG_RETURN_NULL();

  cols = parse_name_to_cols(result);

  /* Process result into Tuple */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                    errmsg("function returning record called in context "
                           "that cannot accept type record")));
  }

  tupdesc = BlessTupleDesc(tupdesc);

  /* Initialize nulls */
  for (i = 0; i < 10; i++)
    nulls[i] = true;

  if (cols->prefix) {
    values[0] = CStringGetTextDatum(cols->prefix);
    nulls[0] = false;
  }
  if (cols->given_name) {
    values[1] = CStringGetTextDatum(cols->given_name);
    nulls[1] = false;
  }
  if (cols->middle_name) {
    values[2] = CStringGetTextDatum(cols->middle_name);
    nulls[2] = false;
  }
  if (cols->surname) {
    values[3] = CStringGetTextDatum(cols->surname);
    nulls[3] = false;
  }
  if (cols->suffix) {
    values[4] = CStringGetTextDatum(cols->suffix);
    nulls[4] = false;
  }
  if (cols->nickname) {
    values[5] = CStringGetTextDatum(cols->nickname);
    nulls[5] = false;
  }
  if (cols->corporation_name) {
    values[6] = CStringGetTextDatum(cols->corporation_name);
    nulls[6] = false;
  }
  if (cols->corporation_type) {
    values[7] = CStringGetTextDatum(cols->corporation_type);
    nulls[7] = false;
  }
  if (cols->organization) {
    values[8] = CStringGetTextDatum(cols->organization);
    nulls[8] = false;
  }
  if (cols->other) {
    values[9] = CStringGetTextDatum(cols->other);
    nulls[9] = false;
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);

  free_parsed_name_cols(cols);
  /* result is freed by context cleanup */

  PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}
