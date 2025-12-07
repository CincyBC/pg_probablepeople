/* src/name_parser.c */
#include "postgres.h"
/* Postgres headers first */
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"

#include "crfsuite_wrapper.h"
#include "feature_extractor.h"
#include "name_parser.h"

#include <ctype.h>
#include <string.h>
#include <sys/time.h>

extern MemoryContext crf_memory_context;

/*
 * Simple tokenizer for name strings
 * Splits on whitespace and some punctuation while preserving meaningful
 * punctuation
 */
TokenInfo *tokenize_name_string(const char *input, int *num_tokens) {
  char *input_copy, *token, *saveptr;
  TokenInfo *tokens;
  int capacity = 20; /* Initial capacity */
  int count = 0;
  char *delimiters = " \t\n\r";

  if (input == NULL || num_tokens == NULL) {
    *num_tokens = 0;
    return NULL;
  }

  tokens = (TokenInfo *)palloc(capacity * sizeof(TokenInfo));
  input_copy = pstrdup(input);

  /* Simple whitespace tokenization */
  token = strtok_r(input_copy, delimiters, &saveptr);
  while (token != NULL) {
    /* Resize array if needed */
    if (count >= capacity) {
      capacity *= 2;
      tokens = (TokenInfo *)repalloc(tokens, capacity * sizeof(TokenInfo));
    }

    /* Clean up token (remove extra punctuation) */
    char *clean_token;
    int len;

    clean_token = pstrdup(token);
    len = strlen(clean_token);

    /* Remove trailing punctuation except for meaningful ones */
    while (len > 0 &&
           (clean_token[len - 1] == ',' || clean_token[len - 1] == '.')) {
      if (clean_token[len - 1] == '.' && len > 1 &&
          (strcmp(clean_token, "Jr.") == 0 || strcmp(clean_token, "Sr.") == 0 ||
           strcmp(clean_token, "Dr.") == 0 || strcmp(clean_token, "Mr.") == 0 ||
           strcmp(clean_token, "Ms.") == 0 ||
           strcmp(clean_token, "Inc.") == 0 ||
           strcmp(clean_token, "Corp.") == 0 ||
           strcmp(clean_token, "Co.") == 0 ||
           strcmp(clean_token, "Ltd.") == 0 ||
           strcmp(clean_token, "Esq.") == 0 ||
           strcmp(clean_token, "Mrs.") == 0)) {
        break; /* Keep meaningful dots */
      }
      clean_token[len - 1] = '\0';
      len--;
    }

    if (len > 0) { /* Only add non-empty tokens */
      tokens[count].text = clean_token;
      tokens[count].position = count;
      tokens[count].start_char = 0; /* Simplified for now */
      tokens[count].end_char = len;
      tokens[count].is_first = (count == 0);
      tokens[count].is_last = false; /* Will be set later */
      count++;
    } else {
      pfree(clean_token);
    }

    token = strtok_r(NULL, delimiters, &saveptr);
  }

  /* Set last token flag */
  if (count > 0) {
    tokens[count - 1].is_last = true;
  }

  pfree(input_copy);
  *num_tokens = count;
  return tokens;
}

/*
 * Main name parsing function
 */
ParseResult *parse_name_string(const char *input_text, CRFModel *model) {
  TokenInfo *tokens;
  int num_tokens;
  crfsuite_instance_t *instance;
  int *predicted_labels;
  floatval_t score;
  ParseResult *result;
  struct timeval start_time, end_time;
  CRFErrorCode crf_result;

  if (input_text == NULL || model == NULL || !model->is_loaded) {
    return NULL;
  }

  gettimeofday(&start_time, NULL);

  /* Tokenize input */
  tokens = tokenize_name_string(input_text, &num_tokens);
  if (tokens == NULL || num_tokens == 0) {
    return NULL;
  }

  /* Create CRF instance with features */
  instance = create_crf_instance_from_tokens(tokens, num_tokens, model->attrs);
  if (instance == NULL) {
    free_token_info_array(tokens, num_tokens);
    return NULL;
  }

  /* Perform CRF prediction */
  crf_result = predict_sequence(model, instance, &predicted_labels, &score);
  if (crf_result != CRF_SUCCESS) {
    free_crf_instance(instance);
    free_token_info_array(tokens, num_tokens);
    return NULL;
  }

  /* Create result structure */
  result = (ParseResult *)palloc0(sizeof(ParseResult));
  result->tokens = (Token *)palloc(num_tokens * sizeof(Token));
  result->num_tokens = num_tokens;
  result->overall_confidence = score;
  result->model_version = pstrdup(model->version ? model->version : "unknown");

  /* Map CRF labels to tokens */
  for (int i = 0; i < num_tokens; i++) {
    result->tokens[i].text = pstrdup(tokens[i].text);
    result->tokens[i].label =
        pstrdup(map_crf_label_to_name_component(predicted_labels[i], model));
    result->tokens[i].confidence =
        0.0; /* Individual confidence not computed for now */
    result->tokens[i].start_pos = tokens[i].start_char;
    result->tokens[i].end_pos = tokens[i].end_char;
  }

  gettimeofday(&end_time, NULL);
  result->processing_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                               (end_time.tv_usec - start_time.tv_usec) / 1000;

  /* Cleanup */
  pfree(predicted_labels);
  free_crf_instance(instance);
  free_token_info_array(tokens, num_tokens);

  return result;
}

/*
 * Free token info array
 */
void free_token_info_array(TokenInfo *tokens, int num_tokens) {
  if (tokens == NULL)
    return;

  for (int i = 0; i < num_tokens; i++) {
    if (tokens[i].text != NULL) {
      pfree(tokens[i].text);
    }
  }
  pfree(tokens);
}

/*
 * Map CRF label ID to name component string
 */
const char *map_crf_label_to_name_component(int label_id, CRFModel *model) {
  const char *label_str;

  if (model == NULL || model->labels == NULL) {
    return "Unknown";
  }

  /* Get label string from CRFSuite dictionary */
  if (model->labels->to_string(model->labels, label_id, &label_str) ==
      CRFSUITE_SUCCESS) {
    /* Map standard CRF labels to probablepeople-style labels */
    if (strcmp(label_str, "GIVEN") == 0)
      return "GivenName";
    if (strcmp(label_str, "SURNAME") == 0)
      return "Surname";
    if (strcmp(label_str, "MIDDLE") == 0)
      return "MiddleName";
    if (strcmp(label_str, "PREFIX") == 0)
      return "PrefixMarital";
    if (strcmp(label_str, "SUFFIX") == 0)
      return "SuffixGenerational";
    if (strcmp(label_str, "NICKNAME") == 0)
      return "Nickname";
    if (strcmp(label_str, "TITLE") == 0)
      return "PrefixOther";

    /* Return original label if no mapping found */
    return label_str;
  }

  return "Unknown";
}

/*
 * Convert parse result to JSONB for PostgreSQL
 */
JsonbValue *parse_result_to_jsonb(ParseResult *result) {
  JsonbParseState *state = NULL;
  JsonbValue *json_result;
  JsonbValue key, val;

  if (result == NULL)
    return NULL;

  /* Create main object */
  json_result = (JsonbValue *)palloc(sizeof(JsonbValue));
  json_result->type = jbvObject;

  pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);

  /* Add tokens array */
  key.type = jbvString;
  key.val.string.val = "tokens";
  key.val.string.len = strlen("tokens");
  pushJsonbValue(&state, WJB_KEY, &key);

  pushJsonbValue(&state, WJB_BEGIN_ARRAY, NULL);

  for (int i = 0; i < result->num_tokens; i++) {
    pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);

    /* Token text */
    key.type = jbvString;
    key.val.string.val = "text";
    key.val.string.len = strlen("text");
    pushJsonbValue(&state, WJB_KEY, &key);

    val.type = jbvString;
    val.val.string.val = result->tokens[i].text;
    val.val.string.len = strlen(result->tokens[i].text);
    pushJsonbValue(&state, WJB_VALUE, &val);

    /* Token label */
    key.type = jbvString;
    key.val.string.val = "label";
    key.val.string.len = strlen("label");
    pushJsonbValue(&state, WJB_KEY, &key);

    val.type = jbvString;
    val.val.string.val = result->tokens[i].label;
    val.val.string.len = strlen(result->tokens[i].label);
    pushJsonbValue(&state, WJB_VALUE, &val);

    pushJsonbValue(&state, WJB_END_OBJECT, NULL);
  }

  pushJsonbValue(&state, WJB_END_ARRAY, NULL);

  /* Add metadata */
  key.type = jbvString;
  key.val.string.val = "confidence";
  key.val.string.len = strlen("confidence");
  pushJsonbValue(&state, WJB_KEY, &key);

  val.type = jbvNumeric;
  val.val.numeric = (Numeric)DatumGetPointer(DirectFunctionCall1(
      float4_numeric, Float4GetDatum(result->overall_confidence)));
  pushJsonbValue(&state, WJB_VALUE, &val);

  key.type = jbvString;
  key.val.string.val = "model_version";
  key.val.string.len = strlen("model_version");
  pushJsonbValue(&state, WJB_KEY, &key);

  val.type = jbvString;
  val.val.string.val = result->model_version;
  val.val.string.len = strlen(result->model_version);
  pushJsonbValue(&state, WJB_VALUE, &val);

  json_result = pushJsonbValue(&state, WJB_END_OBJECT, NULL);

  return json_result;
}

/*
 * Helper to append a string to a destination string with a space separator
 */
static void append_name_component(char **dest, const char *src) {
  if (src == NULL || strlen(src) == 0)
    return;

  if (*dest == NULL) {
    *dest = pstrdup(src);
  } else {
    /* Append with space */
    int old_len = strlen(*dest);
    int new_len = old_len + 1 + strlen(src) + 1;
    char *new_str = (char *)palloc(new_len);
    snprintf(new_str, new_len, "%s %s", *dest, src);
    pfree(*dest);
    *dest = new_str;
  }
}

/*
 * Convert ParseResult to column-based structure
 */
ParsedNameCols *parse_name_to_cols(ParseResult *result) {
  ParsedNameCols *cols;

  if (result == NULL)
    return NULL;

  cols = (ParsedNameCols *)palloc0(sizeof(ParsedNameCols));

  for (int i = 0; i < result->num_tokens; i++) {
    char *label = result->tokens[i].label;
    char *text = result->tokens[i].text;

    if (strcmp(label, "PrefixMarital") == 0 ||
        strcmp(label, "PrefixOther") == 0) {
      append_name_component(&cols->prefix, text);
    } else if (strcmp(label, "GivenName") == 0 ||
               strcmp(label, "FirstInitial") == 0) {
      append_name_component(&cols->given_name, text);
    } else if (strcmp(label, "MiddleName") == 0 ||
               strcmp(label, "MiddleInitial") == 0) {
      append_name_component(&cols->middle_name, text);
    } else if (strcmp(label, "Surname") == 0 ||
               strcmp(label, "LastInitial") == 0) {
      append_name_component(&cols->surname, text);
    } else if (strcmp(label, "SuffixGenerational") == 0 ||
               strcmp(label, "SuffixOther") == 0) {
      append_name_component(&cols->suffix, text);
    } else if (strcmp(label, "Nickname") == 0) {
      append_name_component(&cols->nickname, text);
    } else if (strcmp(label, "CorporationName") == 0 ||
               strcmp(label, "ShortForm") == 0) {
      append_name_component(&cols->corporation_name, text);
    } else if (strcmp(label, "CorporationLegalType") == 0) {
      append_name_component(&cols->corporation_type, text);
    } else if (strcmp(label, "CorporationNameOrganization") == 0 ||
               strcmp(label, "CorporationNameAndCompany") == 0 ||
               strcmp(label, "CorporationCommitteeType") == 0 ||
               strcmp(label, "CorporationNameBranchType") == 0 ||
               strcmp(label, "CorporationNameBranchIdentifier") == 0) {
      append_name_component(&cols->organization, text);
    } else {
      /* Everything else goes to 'other' */
      append_name_component(&cols->other, text);
    }
  }

  return cols;
}

void free_parsed_name_cols(ParsedNameCols *cols) {
  if (cols == NULL)
    return;
  if (cols->prefix)
    pfree(cols->prefix);
  if (cols->given_name)
    pfree(cols->given_name);
  if (cols->middle_name)
    pfree(cols->middle_name);
  if (cols->surname)
    pfree(cols->surname);
  if (cols->suffix)
    pfree(cols->suffix);
  if (cols->nickname)
    pfree(cols->nickname);
  if (cols->corporation_name)
    pfree(cols->corporation_name);
  if (cols->corporation_type)
    pfree(cols->corporation_type);
  if (cols->organization)
    pfree(cols->organization);
  if (cols->other)
    pfree(cols->other);
  pfree(cols);
}