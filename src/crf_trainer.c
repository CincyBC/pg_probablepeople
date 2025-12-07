/* src/crf_trainer.c */
#include "crf_trainer.h"
#include "training_data_parser.h"

#include <crfsuite.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Feature extraction - simplified version for standalone training */
#define MAX_FEATURE_NAME_LEN 256
#define MAX_FEATURES_PER_TOKEN 100

typedef struct {
  char name[MAX_FEATURE_NAME_LEN];
  float weight;
} Feature;

typedef struct {
  Feature features[MAX_FEATURES_PER_TOKEN];
  int num_features;
} FeatureList;

/* Initialize default training config */
void init_training_config(TrainingConfig *config) {
  config->c2 = 1.0;
  config->max_iterations = 100;
  config->epsilon = 0.0001;
}

/* Add feature to list */
static void add_feature(FeatureList *list, const char *name, float weight) {
  if (list->num_features >= MAX_FEATURES_PER_TOKEN)
    return;
  strncpy(list->features[list->num_features].name, name,
          MAX_FEATURE_NAME_LEN - 1);
  list->features[list->num_features].name[MAX_FEATURE_NAME_LEN - 1] = '\0';
  list->features[list->num_features].weight = weight;
  list->num_features++;
}

/* Extract features for a single token - matches feature_extractor.c */
static void extract_token_features(const char *token, FeatureList *features) {
  char buf[MAX_FEATURE_NAME_LEN];
  int len = strlen(token);

  /* Token identity */
  snprintf(buf, sizeof(buf), "token:%s", token);
  add_feature(features, buf, 1.0);

  /* Lowercase token */
  char lower[256];
  for (int i = 0; i < len && i < 255; i++) {
    lower[i] = tolower((unsigned char)token[i]);
  }
  lower[len < 255 ? len : 255] = '\0';

  snprintf(buf, sizeof(buf), "token_lower:%s", lower);
  add_feature(features, buf, 1.0);

  /* No punctuation version */
  char nopunc[256];
  int j = 0;
  for (int i = 0; i < len && j < 254; i++) {
    if (!ispunct((unsigned char)lower[i])) {
      nopunc[j++] = lower[i];
    }
  }
  nopunc[j] = '\0';
  if (j > 0) {
    snprintf(buf, sizeof(buf), "nopunc:%s", nopunc);
    add_feature(features, buf, 1.0);
  }
}

/* Extract prefix/suffix features */
static void extract_affix_features(const char *token, FeatureList *features) {
  char buf[MAX_FEATURE_NAME_LEN];
  int len = strlen(token);

  /* Lowercase and strip punctuation first */
  char clean[256];
  int j = 0;
  for (int i = 0; i < len && j < 254; i++) {
    unsigned char c = (unsigned char)token[i];
    if (!ispunct(c)) {
      clean[j++] = tolower(c);
    }
  }
  clean[j] = '\0';
  len = j;

  /* Prefix features (1-4 chars) */
  for (int plen = 1; plen <= 4 && plen <= len; plen++) {
    char prefix[8];
    strncpy(prefix, clean, plen);
    prefix[plen] = '\0';
    snprintf(buf, sizeof(buf), "prefix_%d:%s", plen, prefix);
    add_feature(features, buf, 1.0);
  }

  /* Suffix features (1-4 chars) */
  for (int slen = 1; slen <= 4 && slen <= len; slen++) {
    char suffix[8];
    strncpy(suffix, clean + len - slen, slen);
    suffix[slen] = '\0';
    snprintf(buf, sizeof(buf), "suffix_%d:%s", slen, suffix);
    add_feature(features, buf, 1.0);
  }
}

/* Extract case features */
static void extract_case_features(const char *token, FeatureList *features) {
  int len = strlen(token);
  if (len == 0)
    return;

  int upper_count = 0, lower_count = 0;

  for (int i = 0; i < len; i++) {
    unsigned char c = (unsigned char)token[i];
    if (isupper(c))
      upper_count++;
    else if (islower(c))
      lower_count++;
  }

  if (isupper((unsigned char)token[0]))
    add_feature(features, "is_capitalized", 1.0);
  if (upper_count == len)
    add_feature(features, "is_all_caps", 1.0);
  if (lower_count == len)
    add_feature(features, "is_all_lower", 1.0);
}

/* Extract length features */
static void extract_length_features(const char *token, FeatureList *features) {
  char buf[MAX_FEATURE_NAME_LEN];
  int len = strlen(token);

  snprintf(buf, sizeof(buf), "length:%d", len);
  add_feature(features, buf, 1.0);

  if (len == 1)
    add_feature(features, "is_single_char", 1.0);
  else if (len == 2)
    add_feature(features, "is_two_char", 1.0);
  else if (len <= 4)
    add_feature(features, "is_short", 1.0);
  else if (len >= 10)
    add_feature(features, "is_long", 1.0);
}

/* Extract character features */
static void extract_char_features(const char *token, FeatureList *features) {
  int len = strlen(token);
  int has_digit = 0, has_punct = 0, has_hyphen = 0, has_dot = 0;

  for (int i = 0; i < len; i++) {
    unsigned char c = (unsigned char)token[i];
    if (isdigit(c))
      has_digit = 1;
    if (ispunct(c))
      has_punct = 1;
    if (c == '-')
      has_hyphen = 1;
    if (c == '.')
      has_dot = 1;
  }

  if (has_digit)
    add_feature(features, "has_digit", 1.0);
  if (has_punct)
    add_feature(features, "has_punct", 1.0);
  if (has_hyphen)
    add_feature(features, "has_hyphen", 1.0);
  if (has_dot)
    add_feature(features, "has_dot", 1.0);

  /* Check if ends with period (abbreviation) */
  if (len > 1 && token[len - 1] == '.')
    add_feature(features, "ends_with_dot", 1.0);
}

/* Extract context features */
static void extract_context_features(LabeledToken *tokens, int num_tokens,
                                     int position, FeatureList *features) {
  char buf[MAX_FEATURE_NAME_LEN];
  int window = 2;

  /* Previous tokens */
  for (int i = 1; i <= window; i++) {
    int prev_pos = position - i;
    if (prev_pos >= 0) {
      snprintf(buf, sizeof(buf), "prev_%d=%s", i, tokens[prev_pos].text);
      add_feature(features, buf, 0.8);
    } else {
      snprintf(buf, sizeof(buf), "prev_%d=BOS", i);
      add_feature(features, buf, 0.5);
    }
  }

  /* Next tokens */
  for (int i = 1; i <= window; i++) {
    int next_pos = position + i;
    if (next_pos < num_tokens) {
      snprintf(buf, sizeof(buf), "next_%d=%s", i, tokens[next_pos].text);
      add_feature(features, buf, 0.8);
    } else {
      snprintf(buf, sizeof(buf), "next_%d=EOS", i);
      add_feature(features, buf, 0.5);
    }
  }
}

/* Extract position features */
static void extract_position_features(int position, int total,
                                      FeatureList *features) {
  if (position == 0)
    add_feature(features, "is_first", 1.0);
  if (position == total - 1)
    add_feature(features, "is_last", 1.0);
}

/* Extract all features for a token */
static void extract_all_features(LabeledToken *tokens, int num_tokens,
                                 int position, FeatureList *features) {
  features->num_features = 0;

  const char *token = tokens[position].text;

  extract_token_features(token, features);
  extract_affix_features(token, features);
  extract_case_features(token, features);
  extract_length_features(token, features);
  extract_char_features(token, features);
  extract_context_features(tokens, num_tokens, position, features);
  extract_position_features(position, num_tokens, features);

  /* Bias feature */
  add_feature(features, "bias", 1.0);
}

/* Logging callback for training progress */
static int training_callback(void *user, const char *format, va_list args) {
  vprintf(format, args);
  fflush(stdout);
  return 0;
}

/* Train CRF model from training data */
int train_crf_model(TrainingData *data, const char *output_file,
                    TrainingConfig *config) {
  crfsuite_data_t crf_data;
  crfsuite_trainer_t *trainer = NULL;
  crfsuite_params_t *params = NULL;
  int ret = -1;

  printf("Training CRF model with %d sequences...\n", data->num_sequences);

  /* Initialize CRF data */
  memset(&crf_data, 0, sizeof(crf_data));

  /* Create dictionaries - use crfsuite_dictionary_create_instance directly */
  if (crfsuite_dictionary_create_instance("dictionary",
                                          (void **)&crf_data.attrs) != 0) {
    fprintf(stderr, "Error: Failed to create attribute dictionary\n");
    goto cleanup;
  }
  if (crfsuite_dictionary_create_instance("dictionary",
                                          (void **)&crf_data.labels) != 0) {
    fprintf(stderr, "Error: Failed to create label dictionary\n");
    goto cleanup;
  }

  /* Allocate instances array */
  crf_data.cap_instances = data->num_sequences;
  crf_data.instances =
      calloc(crf_data.cap_instances, sizeof(crfsuite_instance_t));
  if (!crf_data.instances) {
    fprintf(stderr, "Error: Out of memory\n");
    goto cleanup;
  }

  /* Convert training data to CRFSuite format */
  printf("Converting training data...\n");

  for (int i = 0; i < data->num_sequences; i++) {
    LabeledSequence *seq = &data->sequences[i];
    crfsuite_instance_t *inst = &crf_data.instances[crf_data.num_instances];

    crfsuite_instance_init(inst);

    for (int j = 0; j < seq->num_tokens; j++) {
      crfsuite_item_t item;
      crfsuite_item_init(&item);

      /* Extract features */
      FeatureList features;
      extract_all_features(seq->tokens, seq->num_tokens, j, &features);

      /* Add features to item */
      for (int k = 0; k < features.num_features; k++) {
        crfsuite_attribute_t attr;
        int aid =
            crf_data.attrs->get(crf_data.attrs, features.features[k].name);

        crfsuite_attribute_init(&attr);
        crfsuite_attribute_set(&attr, aid, features.features[k].weight);
        crfsuite_item_append_attribute(&item, &attr);
      }

      /* Get label ID */
      int lid = crf_data.labels->get(crf_data.labels, seq->tokens[j].label);

      /* Add item to instance */
      crfsuite_instance_append(inst, &item, lid);
      crfsuite_item_finish(&item);
    }

    crf_data.num_instances++;

    /* Progress indicator */
    if ((i + 1) % 500 == 0 || i == data->num_sequences - 1) {
      printf("  Processed %d/%d sequences\n", i + 1, data->num_sequences);
    }
  }

  printf("Created %d training instances\n", crf_data.num_instances);
  printf("Attributes: %d, Labels: %d\n", crf_data.attrs->num(crf_data.attrs),
         crf_data.labels->num(crf_data.labels));

  /* Create trainer - crfsuite_create_instance returns 1 on success, 0 on
   * failure */
  if (crfsuite_create_instance("train/crf1d/l2sgd", (void **)&trainer) == 0) {
    fprintf(stderr, "Error: Failed to create L2SGD trainer\n");
    goto cleanup;
  }

  /* Set training parameters */
  params = trainer->params(trainer);
  params->set_float(params, "c2", config->c2);
  params->set_int(params, "max_iterations", config->max_iterations);
  params->set_float(params, "epsilon", config->epsilon);

  /* Set logging callback */
  trainer->set_message_callback(trainer, NULL, training_callback);

  /* Train! */
  printf("\nStarting training with L2SGD algorithm...\n");
  printf("  C2 regularization: %.4f\n", config->c2);
  printf("  Max iterations: %d\n", config->max_iterations);
  printf("  Epsilon: %.6f\n\n", config->epsilon);

  ret = trainer->train(trainer, &crf_data, output_file, -1);

  if (ret == 0) {
    printf("\nTraining completed successfully!\n");
    printf("Model saved to: %s\n", output_file);
  } else {
    fprintf(stderr, "\nTraining failed with error code: %d\n", ret);
  }

cleanup:
  /* Free resources */
  if (trainer)
    trainer->release(trainer);

  for (int i = 0; i < crf_data.num_instances; i++) {
    crfsuite_instance_finish(&crf_data.instances[i]);
  }
  free(crf_data.instances);

  if (crf_data.attrs)
    crf_data.attrs->release(crf_data.attrs);
  if (crf_data.labels)
    crf_data.labels->release(crf_data.labels);

  return ret;
}

/* Train generic model from both person and company data */
int train_generic_model(const char *person_file, const char *company_file,
                        const char *output_file, TrainingConfig *config) {
  /* Parse both files */
  TrainingData *person_data = parse_training_file(person_file);
  TrainingData *company_data = parse_training_file(company_file);

  if (!person_data && !company_data) {
    fprintf(stderr, "Error: Could not parse any training files\n");
    return -1;
  }

  /* Combine data */
  TrainingData combined;
  int total_sequences = (person_data ? person_data->num_sequences : 0) +
                        (company_data ? company_data->num_sequences : 0);

  combined.sequences = malloc(total_sequences * sizeof(LabeledSequence));
  combined.num_sequences = 0;
  combined.capacity = total_sequences;

  if (person_data) {
    for (int i = 0; i < person_data->num_sequences; i++) {
      combined.sequences[combined.num_sequences++] = person_data->sequences[i];
    }
    /* Don't free individual sequences, just the array */
    free(person_data->sequences);
    free(person_data);
  }

  if (company_data) {
    for (int i = 0; i < company_data->num_sequences; i++) {
      combined.sequences[combined.num_sequences++] = company_data->sequences[i];
    }
    free(company_data->sequences);
    free(company_data);
  }

  printf("Combined training data: %d sequences\n", combined.num_sequences);

  int ret = train_crf_model(&combined, output_file, config);

  /* Free combined data */
  for (int i = 0; i < combined.num_sequences; i++) {
    for (int j = 0; j < combined.sequences[i].num_tokens; j++) {
      free(combined.sequences[i].tokens[j].text);
      free(combined.sequences[i].tokens[j].label);
    }
    free(combined.sequences[i].tokens);
  }
  free(combined.sequences);

  return ret;
}
