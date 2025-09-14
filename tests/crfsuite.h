#ifndef CRFSUITE_MOCK_H
#define CRFSUITE_MOCK_H

#include <stdio.h>
#include <stdlib.h>

#define CRFSUITE_SUCCESS 0

typedef float floatval_t;

typedef struct { int dummy; } crfsuite_item_t;
typedef struct { int dummy; } crfsuite_attribute_t;
typedef struct { int num_items; } crfsuite_instance_t;
typedef void crfsuite_tagger_t;
typedef struct { int dummy; } crfsuite_item_t;
typedef struct { int dummy; } crfsuite_attribute_t;

int crfsuite_model_new(crfsuite_model_t **model);
int crfsuite_model_open(crfsuite_model_t *model, const char *filename);
int crfsuite_model_get_labels(crfsuite_model_t *model, crfsuite_dictionary_t **labels);
int crfsuite_model_get_attrs(crfsuite_model_t *model, crfsuite_dictionary_t **attrs);
void crfsuite_model_delete(crfsuite_model_t *model);

int crfsuite_dictionary_to_string(crfsuite_dictionary_t *dic, int id, const char **str);
void crfsuite_dictionary_delete(crfsuite_dictionary_t *dic);

int crfsuite_instance_init(crfsuite_instance_t *inst);
int crfsuite_instance_append(crfsuite_instance_t *inst, const crfsuite_item_t *item);
void crfsuite_instance_finish(crfsuite_instance_t *inst);
int crfsuite_instance_length(crfsuite_instance_t *inst);

int crfsuite_item_init(crfsuite_item_t *item);
int crfsuite_item_append(crfsuite_item_t *item, const crfsuite_attribute_t *attr);
void crfsuite_item_finish(crfsuite_item_t *item);

int crfsuite_attribute_init(crfsuite_attribute_t *attr);
int crfsuite_attribute_set(crfsuite_attribute_t *attr, const char *name, floatval_t value);
void crfsuite_attribute_finish(crfsuite_attribute_t *attr);

int crfsuite_model_get_tagger(crfsuite_model_t *model, crfsuite_tagger_t **tagger);
int crfsuite_tagger_set(crfsuite_tagger_t *tagger, crfsuite_instance_t *inst);
int crfsuite_tagger_viterbi(crfsuite_tagger_t *tagger, int *labels, floatval_t *score);
void crfsuite_tagger_delete(crfsuite_tagger_t *tagger);

#endif // CRFSUITE_MOCK_H
