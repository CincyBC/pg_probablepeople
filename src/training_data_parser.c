/* src/training_data_parser.c */
#include "training_data_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initial capacity for sequences array */
#define INITIAL_CAPACITY 1000

/* Helper function to allocate and copy string */
static char *strdup_safe(const char *s) {
  if (s == NULL)
    return NULL;
  char *copy = malloc(strlen(s) + 1);
  if (copy)
    strcpy(copy, s);
  return copy;
}

/* Skip whitespace in string */
static const char *skip_whitespace(const char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

/* Find closing tag, return pointer after '>' */
static const char *find_tag_end(const char *s) {
  while (*s && *s != '>')
    s++;
  if (*s == '>')
    return s + 1;
  return NULL;
}

/* Extract tag name from "<TagName>" or "</TagName>" */
static int extract_tag_name(const char *start, char *buf, size_t bufsize,
                            bool *is_closing) {
  const char *p = start;
  size_t i = 0;

  if (*p != '<')
    return -1;
  p++;

  *is_closing = false;
  if (*p == '/') {
    *is_closing = true;
    p++;
  }

  while (*p && *p != '>' && *p != ' ' && i < bufsize - 1) {
    buf[i++] = *p++;
  }
  buf[i] = '\0';

  return (i > 0) ? 0 : -1;
}

/* Extract text content between current position and next '<' */
static char *extract_text_content(const char *start, const char **endptr) {
  const char *p = start;
  const char *text_start = p;

  /* Find end of text (next tag or end) */
  while (*p && *p != '<')
    p++;

  if (p == text_start) {
    *endptr = p;
    return NULL;
  }

  /* Copy and trim whitespace */
  size_t len = p - text_start;
  char *text = malloc(len + 1);
  if (!text) {
    *endptr = p;
    return NULL;
  }
  memcpy(text, text_start, len);
  text[len] = '\0';

  /* Trim leading/trailing whitespace */
  char *trimmed = text;
  while (*trimmed && isspace((unsigned char)*trimmed))
    trimmed++;

  char *end = trimmed + strlen(trimmed) - 1;
  while (end > trimmed && isspace((unsigned char)*end))
    *end-- = '\0';

  if (trimmed != text) {
    char *result = strdup_safe(trimmed);
    free(text);
    *endptr = p;
    return result;
  }

  *endptr = p;
  return text;
}

/* Add a sequence to the training data */
static int add_sequence(TrainingData *data, LabeledSequence *seq) {
  if (data->num_sequences >= data->capacity) {
    int new_capacity = data->capacity * 2;
    LabeledSequence *new_seqs =
        realloc(data->sequences, new_capacity * sizeof(LabeledSequence));
    if (!new_seqs)
      return -1;
    data->sequences = new_seqs;
    data->capacity = new_capacity;
  }
  data->sequences[data->num_sequences++] = *seq;
  return 0;
}

/* Parse a single <Name>...</Name> element */
static int parse_name_element(const char *start, const char **endptr,
                              LabeledSequence *seq) {
  const char *p = start;
  char tag_name[64];
  bool is_closing;

  seq->tokens = malloc(MAX_TOKENS_PER_NAME * sizeof(LabeledToken));
  if (!seq->tokens)
    return -1;
  seq->num_tokens = 0;

  /* Skip to content after <Name> */
  p = find_tag_end(p);
  if (!p)
    return -1;

  /* Parse tokens until </Name> */
  while (*p) {
    p = skip_whitespace(p);

    if (*p == '<') {
      /* Check for closing </Name> */
      if (extract_tag_name(p, tag_name, sizeof(tag_name), &is_closing) == 0) {
        if (is_closing && strcmp(tag_name, "Name") == 0) {
          /* Found </Name>, done with this sequence */
          p = find_tag_end(p);
          *endptr = p;
          return 0;
        }

        if (!is_closing) {
          /* This is a label tag like <GivenName>, <Surname>, etc. */
          char *label = strdup_safe(tag_name);

          /* Move past opening tag */
          p = find_tag_end(p);
          if (!p) {
            free(label);
            return -1;
          }

          /* Extract text content */
          char *text = extract_text_content(p, &p);

          /* Skip closing tag */
          if (*p == '<') {
            char close_tag[64];
            bool closing;
            if (extract_tag_name(p, close_tag, sizeof(close_tag), &closing) ==
                    0 &&
                closing) {
              p = find_tag_end(p);
            }
          }

          /* Add token if we have text */
          if (text && strlen(text) > 0 &&
              seq->num_tokens < MAX_TOKENS_PER_NAME) {
            seq->tokens[seq->num_tokens].text = text;
            seq->tokens[seq->num_tokens].label = label;
            seq->num_tokens++;
          } else {
            free(text);
            free(label);
          }
        } else {
          /* Unexpected closing tag, skip */
          p = find_tag_end(p);
        }
      } else {
        p++;
      }
    } else if (*p) {
      /* Non-tag content (whitespace between tokens), skip */
      p++;
    }
  }

  *endptr = p;
  return 0;
}

/* Parse entire XML file */
TrainingData *parse_training_file(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "Error: Cannot open file %s\n", filename);
    return NULL;
  }

  /* Read entire file into memory */
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *content = malloc(file_size + 1);
  if (!content) {
    fclose(fp);
    return NULL;
  }

  size_t read_size = fread(content, 1, file_size, fp);
  content[read_size] = '\0';
  fclose(fp);

  /* Allocate training data structure */
  TrainingData *data = malloc(sizeof(TrainingData));
  if (!data) {
    free(content);
    return NULL;
  }

  data->sequences = malloc(INITIAL_CAPACITY * sizeof(LabeledSequence));
  if (!data->sequences) {
    free(data);
    free(content);
    return NULL;
  }
  data->num_sequences = 0;
  data->capacity = INITIAL_CAPACITY;

  /* Parse content */
  const char *p = content;
  char tag_name[64];
  bool is_closing;

  while (*p) {
    p = skip_whitespace(p);

    if (*p == '<') {
      if (extract_tag_name(p, tag_name, sizeof(tag_name), &is_closing) == 0) {
        if (!is_closing && strcmp(tag_name, "Name") == 0) {
          /* Found <Name> element */
          LabeledSequence seq;
          memset(&seq, 0, sizeof(seq));

          if (parse_name_element(p, &p, &seq) == 0 && seq.num_tokens > 0) {
            add_sequence(data, &seq);
          } else {
            /* Free failed sequence */
            for (int i = 0; i < seq.num_tokens; i++) {
              free(seq.tokens[i].text);
              free(seq.tokens[i].label);
            }
            free(seq.tokens);
          }
        } else {
          /* Skip other tags */
          p = find_tag_end(p);
          if (!p)
            break;
        }
      } else {
        p++;
      }
    } else {
      p++;
    }
  }

  free(content);
  return data;
}

/* Free training data */
void free_training_data(TrainingData *data) {
  if (!data)
    return;

  for (int i = 0; i < data->num_sequences; i++) {
    LabeledSequence *seq = &data->sequences[i];
    for (int j = 0; j < seq->num_tokens; j++) {
      free(seq->tokens[j].text);
      free(seq->tokens[j].label);
    }
    free(seq->tokens);
  }
  free(data->sequences);
  free(data);
}

/* Print training data summary */
void print_training_summary(TrainingData *data) {
  if (!data)
    return;

  printf("Training data summary:\n");
  printf("  Total sequences: %d\n", data->num_sequences);

  int total_tokens = 0;
  for (int i = 0; i < data->num_sequences; i++) {
    total_tokens += data->sequences[i].num_tokens;
  }
  printf("  Total tokens: %d\n", total_tokens);

  /* Print first few examples */
  printf("\nFirst 5 examples:\n");
  for (int i = 0; i < data->num_sequences && i < 5; i++) {
    LabeledSequence *seq = &data->sequences[i];
    printf("  [%d] ", i + 1);
    for (int j = 0; j < seq->num_tokens; j++) {
      printf("%s/%s ", seq->tokens[j].text, seq->tokens[j].label);
    }
    printf("\n");
  }
}
