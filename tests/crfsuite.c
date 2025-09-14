#include "crfsuite.h"

int crfsuite_model_new(crfsuite_model_t **model) {
    *model = (crfsuite_model_t *)malloc(sizeof(crfsuite_model_t));
    return CRFSUITE_SUCCESS;
}

int crfsuite_model_open(crfsuite_model_t *model, const char *filename) {
    return CRFSUITE_SUCCESS;
}

int crfsuite_model_get_labels(crfsuite_model_t *model, crfsuite_dictionary_t **labels) {
    *labels = (crfsuite_dictionary_t *)malloc(sizeof(crfsuite_dictionary_t));
    return CRFSUITE_SUCCESS;
}

int crfsuite_model_get_attrs(crfsuite_model_t *model, crfsuite_dictionary_t **attrs) {
    *attrs = (crfsuite_dictionary_t *)malloc(sizeof(crfsuite_dictionary_t));
    return CRFSUITE_SUCCESS;
}

void crfsuite_model_delete(crfsuite_model_t *model) {
    free(model);
}

int crfsuite_dictionary_to_string(crfsuite_dictionary_t *dic, int id, const char **str) {
    *str = "MOCK_LABEL";
    return CRFSUITE_SUCCESS;
}

void crfsuite_dictionary_delete(crfsuite_dictionary_t *dic) {
    free(dic);
}

int crfsuite_instance_init(crfsuite_instance_t *inst) {
    inst->num_items = 0;
    return CRFSUITE_SUCCESS;
}

int crfsuite_instance_append(crfsuite_instance_t *inst, const crfsuite_item_t *item) {
    inst->num_items++;
    return CRFSUITE_SUCCESS;
}

void crfsuite_instance_finish(crfsuite_instance_t *inst) {
    // no-op
}

int crfsuite_instance_length(crfsuite_instance_t *inst) {
    return inst->num_items;
}

int crfsuite_item_init(crfsuite_item_t *item) {
    return CRFSUITE_SUCCESS;
}

int crfsuite_item_append(crfsuite_item_t *item, const crfsuite_attribute_t *attr) {
    return CRFSUITE_SUCCESS;
}

void crfsuite_item_finish(crfsuite_item_t *item) {
    // no-op
}

int crfsuite_attribute_init(crfsuite_attribute_t *attr) {
    return CRFSUITE_SUCCESS;
}

int crfsuite_attribute_set(crfsuite_attribute_t *attr, const char *name, floatval_t value) {
    return CRFSUITE_SUCCESS;
}

void crfsuite_attribute_finish(crfsuite_attribute_t *attr) {
    // no-op
}

int crfsuite_model_get_tagger(crfsuite_model_t *model, crfsuite_tagger_t **tagger) {
    *tagger = (crfsuite_tagger_t *)malloc(sizeof(crfsuite_tagger_t));
    return CRFSUITE_SUCCESS;
}

int crfsuite_tagger_set(crfsuite_tagger_t *tagger, crfsuite_instance_t *inst) {
    return CRFSUITE_SUCCESS;
}

int crfsuite_tagger_viterbi(crfsuite_tagger_t *tagger, int *labels, floatval_t *score) {
    *score = 0.95;
    // just fill labels with 0, 1, 2, ...
    for (int i = 0; i < 5; i++) { // assume max 5 labels for mock
        labels[i] = i;
    }
    return CRFSUITE_SUCCESS;
}

void crfsuite_tagger_delete(crfsuite_tagger_t *tagger) {
    free(tagger);
}
