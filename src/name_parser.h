/* src/name_parser.h */
#ifndef NAME_PARSER_H
#define NAME_PARSER_H

#include "crfsuite_wrapper.h"
#include "feature_extractor.h"
#include "postgres.h"

/* Name parsing functions */
ParseResult *parse_name_string(const char *input_text, CRFModel *model);
TokenInfo *tokenize_name_string(const char *input, int *num_tokens);
void free_token_info_array(TokenInfo *tokens, int num_tokens);

/* JSON conversion */
JsonbValue *parse_result_to_jsonb(ParseResult *result);

/* Caching functions - Removed */

/* Label mapping */
const char *map_crf_label_to_name_component(int label_id, CRFModel *model);

#endif /* NAME_PARSER_H */