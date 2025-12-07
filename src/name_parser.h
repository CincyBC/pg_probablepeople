/* src/name_parser.h */
#ifndef NAME_PARSER_H
#define NAME_PARSER_H

#include "crfsuite_wrapper.h"
#include "feature_extractor.h"
#include "postgres.h"

/* Result structure for column-based parsing */
typedef struct {
  char *prefix;
  char *given_name;
  char *middle_name;
  char *surname;
  char *suffix;
  char *nickname;
  char *corporation_name;
  char *corporation_type;
  char *organization;
  char *other;
} ParsedNameCols;

/* Core parsing functions */
ParseResult *parse_name_string(const char *input_text, CRFModel *model);
const char *map_crf_label_to_name_component(int label_id, CRFModel *model);
JsonbValue *parse_result_to_jsonb(ParseResult *result);
ParsedNameCols *parse_name_to_cols(ParseResult *result);
void free_parsed_name_cols(ParsedNameCols *cols);

/* Helper functions */
TokenInfo *tokenize_name_string(const char *input, int *num_tokens);
void free_token_info_array(TokenInfo *tokens, int num_tokens);

/* Caching functions - Removed */

/* Label mapping */
const char *map_crf_label_to_name_component(int label_id, CRFModel *model);

#endif /* NAME_PARSER_H */