/* src/feature_extractor.h */
#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include "postgres.h"
#include "utils/memutils.h"
#include <crfsuite.h>

/* Feature types */
#define MAX_FEATURE_NAME_LEN 128
#define MAX_TOKEN_LEN 256
#define FEATURE_WINDOW_SIZE 3

typedef struct {
  char name[MAX_FEATURE_NAME_LEN];
  float weight;
} Feature;

typedef struct {
  Feature *features;
  int num_features;
  int capacity;
} FeatureSet;

/* Token information for feature extraction */
typedef struct {
  char *text;
  int position;
  int start_char;
  int end_char;
  bool is_first;
  bool is_last;
} TokenInfo;

/* Feature extraction functions */
FeatureSet *create_feature_set(void);
void free_feature_set(FeatureSet *fs);
void add_feature(FeatureSet *fs, const char *name, float weight);

/* Core feature extractors */
void extract_token_features(TokenInfo *token, FeatureSet *features);
void extract_shape_features(TokenInfo *token, FeatureSet *features);
void extract_prefix_suffix_features(TokenInfo *token, FeatureSet *features);
void extract_case_features(TokenInfo *token, FeatureSet *features);
void extract_length_features(TokenInfo *token, FeatureSet *features);
void extract_character_features(TokenInfo *token, FeatureSet *features);
void extract_context_features(TokenInfo *tokens, int num_tokens, int position,
                              FeatureSet *features);
void extract_position_features(TokenInfo *token, int total_tokens,
                               FeatureSet *features);

/*
 * Create a CRFSuite instance from token sequence
 */
crfsuite_instance_t *
create_crf_instance_from_tokens(TokenInfo *tokens, int num_tokens,
                                crfsuite_dictionary_t *attrs);
void free_crf_instance(crfsuite_instance_t *instance);

/* Utility functions */
char *get_token_shape(const char *token);
bool is_capitalized(const char *token);
bool is_all_caps(const char *token);
bool is_all_lower(const char *token);
bool has_digit(const char *token);
bool has_punctuation(const char *token);
bool is_numeric(const char *token);
char *get_prefix(const char *token, int length);
char *get_suffix(const char *token, int length);

#endif /* FEATURE_EXTRACTOR_H */