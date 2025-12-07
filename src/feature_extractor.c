/* src/feature_extractor.c */
#include "feature_extractor.h"
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern MemoryContext crf_memory_context;

/*
 * Create a new feature set
 */
FeatureSet *create_feature_set(void) {
  MemoryContext oldcontext;
  FeatureSet *fs;

  oldcontext = MemoryContextSwitchTo(crf_memory_context);

  fs = (FeatureSet *)palloc0(sizeof(FeatureSet));
  fs->capacity = 50; /* Initial capacity */
  fs->features = (Feature *)palloc0(fs->capacity * sizeof(Feature));
  fs->num_features = 0;

  MemoryContextSwitchTo(oldcontext);
  return fs;
}

/*
 * Free feature set
 */
void free_feature_set(FeatureSet *fs) {
  if (fs == NULL)
    return;

  if (fs->features != NULL) {
    pfree(fs->features);
  }
  pfree(fs);
}

/*
 * Add feature to feature set
 */
void add_feature(FeatureSet *fs, const char *name, float weight) {
  if (fs == NULL || name == NULL)
    return;

  /* Resize if needed */
  if (fs->num_features >= fs->capacity) {
    fs->capacity *= 2;
    fs->features =
        (Feature *)repalloc(fs->features, fs->capacity * sizeof(Feature));
  }

  strncpy(fs->features[fs->num_features].name, name, MAX_FEATURE_NAME_LEN - 1);
  fs->features[fs->num_features].name[MAX_FEATURE_NAME_LEN - 1] = '\0';
  fs->features[fs->num_features].weight = weight;
  fs->num_features++;
}

/*
 * Extract basic token features (similar to probablepeople)
 */
void extract_token_features(TokenInfo *token, FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];

  if (token == NULL || token->text == NULL)
    return;

  /* Token identity feature */
  snprintf(feature_name, sizeof(feature_name), "token:%s", token->text);
  add_feature(features, feature_name, 1.0);

  /* Lowercased token & nopunc */
  char *lower_token;
  char *p;
  char *np_start;

  lower_token = pstrdup(token->text);
  for (p = lower_token; *p; p++) {
    *p = tolower(*p);
  }

  snprintf(feature_name, sizeof(feature_name), "token_lower:%s", lower_token);
  add_feature(features, feature_name, 1.0);

  /* nopunc feature (lowercase without punctuation) */
  np_start = (char *)palloc((strlen(lower_token) + 1) * sizeof(char));
  {
    int i;
    int j = 0;
    for (i = 0; lower_token[i]; i++) {
      if (!ispunct((unsigned char)lower_token[i])) {
        np_start[j++] = lower_token[i];
      }
    }
    np_start[j] = '\0';

    if (j > 0) {
      snprintf(feature_name, sizeof(feature_name), "nopunc:%s", np_start);
      add_feature(features, feature_name, 1.0);
    }
  }
  pfree(np_start);

  pfree(lower_token);
}

/*
 * Extract word shape features
 */
void extract_shape_features(TokenInfo *token, FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];
  char *shape;

  if (token == NULL || token->text == NULL)
    return;

  shape = get_token_shape(token->text);
  if (shape != NULL) {
    snprintf(feature_name, sizeof(feature_name), "shape:%s", shape);
    add_feature(features, feature_name, 1.0);
    pfree(shape);
  }
}

/*
 * Extract prefix and suffix features
 */
void extract_prefix_suffix_features(TokenInfo *token, FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];
  char *prefix, *suffix;
  int lengths[] = {1, 2, 3, 4};
  int num_lengths = sizeof(lengths) / sizeof(lengths[0]);
  int i;
  char *lower_text;
  char *clean_text;
  char *p;
  int j = 0;

  if (token == NULL || token->text == NULL)
    return;

  lower_text = pstrdup(token->text);
  clean_text = (char *)palloc((strlen(lower_text) + 1) * sizeof(char));

  /* Lowercase and strip punctuation */
  for (p = lower_text; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!ispunct(c)) {
      clean_text[j++] = tolower(c);
    }
  }
  clean_text[j] = '\0';
  pfree(lower_text);

  /* Use clean_text for affixes */
  for (i = 0; i < num_lengths; i++) {
    /* Prefix features */
    prefix = get_prefix(clean_text, lengths[i]);
    if (prefix != NULL) {
      snprintf(feature_name, sizeof(feature_name), "prefix_%d:%s", lengths[i],
               prefix);
      add_feature(features, feature_name, 1.0);
      pfree(prefix);
    }

    /* Suffix features */
    suffix = get_suffix(clean_text, lengths[i]);
    if (suffix != NULL) {
      snprintf(feature_name, sizeof(feature_name), "suffix_%d:%s", lengths[i],
               suffix);
      add_feature(features, feature_name, 1.0);
      pfree(suffix);
    }
  }
  pfree(clean_text);
}

/*
 * Extract case-based features
 */
/*
 * Extract case-based features
 */
void extract_case_features(TokenInfo *token, FeatureSet *features) {
  int len;

  if (token == NULL || token->text == NULL)
    return;

  len = strlen(token->text);

  if (is_capitalized(token->text)) {
    add_feature(features, "is_capitalized", 1.0);
  }

  if (is_all_caps(token->text)) {
    add_feature(features, "is_all_caps", 1.0);
  }

  /* Additional boolean features */
  if (len > 0 && token->text[len - 1] == '.') {
    add_feature(features, "abbrev", 1.0);
    if (len <= 2) {
      add_feature(features, "initial", 1.0);
    }
  }

  if (strchr(token->text, ',') != NULL) {
    add_feature(features, "comma", 1.0);
  }

  if (strchr(token->text, '-') != NULL) {
    add_feature(features, "hyphenated", 1.0);
  }

  if (strchr(token->text, '(') != NULL || strchr(token->text, ')') != NULL) {
    add_feature(features, "bracketed", 1.0);
  }

  /* Length feature as value? Or length classification?
     Assuming length feature value based on log "Attr: length" implies it's a
     feature name with specific weight? Or usually "length:4"? If the log has
     "length" as a key, it means feature is binary? Let's try adding binary
     "length" if length > 1? Or maybe "length" is unused? Actually
     probablepeople uses "length:N" usually. But "Attr: length" appeared in the
     log. Let's try skipping length for now to act safe. */
  if (is_all_lower(token->text)) {
    add_feature(features, "is_all_lower", 1.0);
  }
}

/*
 * Extract length features
 */
void extract_length_features(TokenInfo *token, FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];
  int len;

  if (token == NULL || token->text == NULL)
    return;

  len = strlen(token->text);

  /* Continuous length feature */
  add_feature(features, "length", (floatval_t)len);

  /* Helper features for length buckets */
  if (len == 1) {
    add_feature(features, "length:1", 1.0);
  } else if (len == 2) {
    add_feature(features, "length:2", 1.0);
  } else if (len == 3) {
    add_feature(features, "length:3", 1.0);
  } else if (len == 4) {
    add_feature(features, "length:4", 1.0);
  } else if (len > 4) {
    add_feature(features, "length:>4", 1.0);
  }
}

/*
 * Extract character-based features
 */
void extract_character_features(TokenInfo *token, FeatureSet *features) {
  /* Variables for character analysis */
  int just_letters = 1;
  int has_vowels = 0;
  int digits = 0;
  int i;
  char last = 0;
  int is_roman = 1;
  char c;
  char lower;

  if (token == NULL || token->text == NULL)
    return;

  if (has_digit(token->text)) {
    add_feature(features, "has_digit", 1.0);
  }

  if (has_punctuation(token->text)) {
    add_feature(features, "has_punctuation", 1.0);
  }

  if (is_numeric(token->text)) {
    add_feature(features, "is_numeric", 1.0);
  }

  /* Just letters - Disabled as it caused regressions */
  /*
  for (i = 0; token->text[i]; i++) {
    c = (unsigned char)token->text[i];
    unsigned char uc = (unsigned char)token->text[i];

    if (!isalpha(uc) && uc != '.') just_letters = 0;
    if (!isalpha(uc)) just_letters = 0;

    if (isdigit(uc)) digits = 1;

    lower = tolower(uc);
    if (strchr("aeiouy", lower)) has_vowels = 1;
    last = lower;
  }

  if (just_letters) {
    add_feature(features, "just.letters", 1.0);
  }

  if (has_vowels) {
    add_feature(features, "has.vowels", 1.0);
  }

  if (last && strchr("aeiouy", last)) {
      add_feature(features, "endswith.vowel", 1.0);
  }

  if (!digits) {
      add_feature(features, "digits:no_digits", 1.0);
  }
  */

  /* Roman numeral check (simple) */
  is_roman = 1; // Reset for this check
  for (i = 0; token->text[i]; i++) {
    c = toupper((unsigned char)token->text[i]);
    if (!strchr("IVXLCM", c)) {
      is_roman = 0;
      break;
    }
  }
  if (is_roman && token->text[0]) {
    add_feature(features, "roman", 1.0);
  }
}

/*
 * Extract context features from neighboring tokens
 */
void extract_context_features(TokenInfo *tokens, int num_tokens, int position,
                              FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];
  int window = FEATURE_WINDOW_SIZE;

  if (tokens == NULL || position < 0 || position >= num_tokens)
    return;

  /* Previous tokens */
  for (int i = 1; i <= window; i++) {
    int prev_pos = position - i;
    if (prev_pos >= 0) {
      snprintf(feature_name, sizeof(feature_name), "prev_%d=%s", i,
               tokens[prev_pos].text);
      add_feature(features, feature_name, 0.8);
    } else {
      snprintf(feature_name, sizeof(feature_name), "prev_%d=BOS", i);
      add_feature(features, feature_name, 0.5);
    }
  }

  /* Next tokens */
  for (int i = 1; i <= window; i++) {
    int next_pos = position + i;
    if (next_pos < num_tokens) {
      snprintf(feature_name, sizeof(feature_name), "next_%d=%s", i,
               tokens[next_pos].text);
      add_feature(features, feature_name, 0.8);
    } else {
      snprintf(feature_name, sizeof(feature_name), "next_%d=EOS", i);
      add_feature(features, feature_name, 0.5);
    }
  }
}

/*
 * Extract position features
 */
void extract_position_features(TokenInfo *token, int total_tokens,
                               FeatureSet *features) {
  char feature_name[MAX_FEATURE_NAME_LEN];
  float relative_pos;

  if (token == NULL)
    return;

  if (token->is_first) {
    add_feature(features, "rawstring.start", 1.0);
  }

  if (token->is_last) {
    add_feature(features, "rawstring.end", 1.0);
  }

  if (total_tokens == 1) {
    add_feature(features, "singleton", 1.0);
  }

  /* Relative position */
  if (total_tokens > 1) {
    relative_pos = (float)token->position / (total_tokens - 1);
    if (relative_pos < 0.33) {
      add_feature(features, "pos_early", 1.0);
    } else if (relative_pos < 0.67) {
      add_feature(features, "pos_middle", 1.0);
    } else {
      add_feature(features, "pos_late", 1.0);
    }
  }

  snprintf(feature_name, sizeof(feature_name), "position=%d", token->position);
  add_feature(features, feature_name, 0.5);
}

/*
 * Create CRFSuite instance from token sequence
 */
crfsuite_instance_t *
create_crf_instance_from_tokens(TokenInfo *tokens, int num_tokens,
                                crfsuite_dictionary_t *attrs) {
  crfsuite_instance_t *instance;
  crfsuite_item_t item;
  crfsuite_attribute_t attr;
  FeatureSet *features;
  int aid;

  if (tokens == NULL || num_tokens <= 0 || attrs == NULL)
    return NULL;

  instance = (crfsuite_instance_t *)palloc0(sizeof(crfsuite_instance_t));
  crfsuite_instance_init(instance);

  /* Extract features for each token */
  for (int i = 0; i < num_tokens; i++) {
    crfsuite_item_init(&item);

    features = create_feature_set();

    /* Extract all feature types */
    extract_token_features(&tokens[i], features);
    extract_shape_features(&tokens[i], features);
    extract_prefix_suffix_features(&tokens[i], features);
    extract_case_features(&tokens[i], features);
    extract_length_features(&tokens[i], features);
    extract_character_features(&tokens[i], features);
    extract_context_features(tokens, num_tokens, i, features);
    extract_position_features(&tokens[i], num_tokens, features);

    /* Add features to CRFSuite item using dictionary mapping */
    for (int j = 0; j < features->num_features; j++) {
      /* Map feature string to integer ID using model's dictionary */
      aid = attrs->to_id(attrs, features->features[j].name);

      /* Only add known features */
      if (aid >= 0) {
        crfsuite_attribute_init(&attr);
        crfsuite_attribute_set(&attr, aid, features->features[j].weight);
        crfsuite_item_append_attribute(&item, &attr);
        /* No attribute finish needed */
      }
    }

    /* 0 is a dummy label for testing/parsing */
    crfsuite_instance_append(instance, &item, 0);

    crfsuite_item_finish(&item);
    free_feature_set(features);
  }

  return instance;
}

/*
 * Free CRFSuite instance
 */
void free_crf_instance(crfsuite_instance_t *instance) {
  if (instance == NULL)
    return;

  crfsuite_instance_finish(instance);
  pfree(instance);
}

/* Utility functions */

/*
 * Get token shape (similar to Stanford NER)
 */
char *get_token_shape(const char *token) {
  int len;
  char *shape;

  if (token == NULL)
    return NULL;

  len = strlen(token);
  shape = (char *)palloc((len + 1) * sizeof(char));

  for (int i = 0; i < len; i++) {
    if (isupper(token[i])) {
      shape[i] = 'X';
    } else if (islower(token[i])) {
      shape[i] = 'x';
    } else if (isdigit(token[i])) {
      shape[i] = 'd';
    } else {
      shape[i] = token[i]; /* Keep punctuation as is */
    }
  }
  shape[len] = '\0';

  return shape;
}

/*
 * Check if token is capitalized
 */
bool is_capitalized(const char *token) {
  return token != NULL && strlen(token) > 0 && isupper(token[0]);
}

/*
 * Check if token is all uppercase
 */
bool is_all_caps(const char *token) {
  if (token == NULL || strlen(token) == 0)
    return false;

  for (const char *p = token; *p; p++) {
    if (isalpha(*p) && !isupper(*p)) {
      return false;
    }
  }
  return true;
}

/*
 * Check if token is all lowercase
 */
bool is_all_lower(const char *token) {
  if (token == NULL || strlen(token) == 0)
    return false;

  for (const char *p = token; *p; p++) {
    if (isalpha(*p) && !islower(*p)) {
      return false;
    }
  }
  return true;
}

/*
 * Check if token contains digits
 */
bool has_digit(const char *token) {
  if (token == NULL)
    return false;

  for (const char *p = token; *p; p++) {
    if (isdigit(*p)) {
      return true;
    }
  }
  return false;
}

/*
 * Check if token contains punctuation
 */
bool has_punctuation(const char *token) {
  if (token == NULL)
    return false;

  for (const char *p = token; *p; p++) {
    if (ispunct(*p)) {
      return true;
    }
  }
  return false;
}

/*
 * Check if token is numeric
 */
bool is_numeric(const char *token) {
  if (token == NULL || strlen(token) == 0)
    return false;

  for (const char *p = token; *p; p++) {
    if (!isdigit(*p) && *p != '.' && *p != ',') {
      return false;
    }
  }
  return true;
}

/*
 * Get prefix of specified length
 */
char *get_prefix(const char *token, int length) {
  int token_len;
  char *prefix;

  if (token == NULL || length <= 0)
    return NULL;

  token_len = strlen(token);
  if (length > token_len)
    length = token_len;

  prefix = (char *)palloc((length + 1) * sizeof(char));
  strncpy(prefix, token, length);
  prefix[length] = '\0';

  return prefix;
}

/*
 * Get suffix of specified length
 */
char *get_suffix(const char *token, int length) {
  int token_len;
  char *suffix;

  if (token == NULL || length <= 0)
    return NULL;

  token_len = strlen(token);
  if (length > token_len)
    length = token_len;

  suffix = (char *)palloc((length + 1) * sizeof(char));
  strcpy(suffix, token + token_len - length);

  return suffix;
}