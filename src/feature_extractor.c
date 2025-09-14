/* src/feature_extractor.c */
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "feature_extractor.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

extern MemoryContext crf_memory_context;

/*
 * Create a new feature set
 */
FeatureSet *create_feature_set(void)
{
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
void free_feature_set(FeatureSet *fs)
{
    if (fs == NULL)
        return;

    if (fs->features != NULL)
    {
        pfree(fs->features);
    }
    pfree(fs);
}

/*
 * Add feature to feature set
 */
void add_feature(FeatureSet *fs, const char *name, float weight)
{
    if (fs == NULL || name == NULL)
        return;

    /* Resize if needed */
    if (fs->num_features >= fs->capacity)
    {
        fs->capacity *= 2;
        fs->features = (Feature *)repalloc(fs->features, fs->capacity * sizeof(Feature));
    }

    strncpy(fs->features[fs->num_features].name, name, MAX_FEATURE_NAME_LEN - 1);
    fs->features[fs->num_features].name[MAX_FEATURE_NAME_LEN - 1] = '\0';
    fs->features[fs->num_features].weight = weight;
    fs->num_features++;
}

/*
 * Extract basic token features (similar to probablepeople)
 */
void extract_token_features(TokenInfo *token, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];

    if (token == NULL || token->text == NULL)
        return;

    /* Token identity feature */
    snprintf(feature_name, sizeof(feature_name), "token=%s", token->text);
    add_feature(features, feature_name, 1.0);

    /* Lowercased token */
    char *lower_token = pstrdup(token->text);
    for (char *p = lower_token; *p; p++)
    {
        *p = tolower(*p);
    }
    snprintf(feature_name, sizeof(feature_name), "token_lower=%s", lower_token);
    add_feature(features, feature_name, 1.0);
    pfree(lower_token);
}

/*
 * Extract word shape features
 */
void extract_shape_features(TokenInfo *token, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];
    char *shape;

    if (token == NULL || token->text == NULL)
        return;

    shape = get_token_shape(token->text);
    if (shape != NULL)
    {
        snprintf(feature_name, sizeof(feature_name), "shape=%s", shape);
        add_feature(features, feature_name, 1.0);
        pfree(shape);
    }
}

/*
 * Extract prefix and suffix features
 */
void extract_prefix_suffix_features(TokenInfo *token, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];
    char *prefix, *suffix;
    int lengths[] = {1, 2, 3, 4};
    int num_lengths = sizeof(lengths) / sizeof(lengths[0]);

    if (token == NULL || token->text == NULL)
        return;

    for (int i = 0; i < num_lengths; i++)
    {
        /* Prefix features */
        prefix = get_prefix(token->text, lengths[i]);
        if (prefix != NULL)
        {
            snprintf(feature_name, sizeof(feature_name), "prefix_%d=%s", lengths[i], prefix);
            add_feature(features, feature_name, 1.0);
            pfree(prefix);
        }

        /* Suffix features */
        suffix = get_suffix(token->text, lengths[i]);
        if (suffix != NULL)
        {
            snprintf(feature_name, sizeof(feature_name), "suffix_%d=%s", lengths[i], suffix);
            add_feature(features, feature_name, 1.0);
            pfree(suffix);
        }
    }
}

/*
 * Extract case-based features
 */
void extract_case_features(TokenInfo *token, FeatureSet *features)
{
    if (token == NULL || token->text == NULL)
        return;

    if (is_capitalized(token->text))
    {
        add_feature(features, "is_capitalized", 1.0);
    }

    if (is_all_caps(token->text))
    {
        add_feature(features, "is_all_caps", 1.0);
    }

    if (is_all_lower(token->text))
    {
        add_feature(features, "is_all_lower", 1.0);
    }
}

/*
 * Extract length features
 */
void extract_length_features(TokenInfo *token, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];
    int len;

    if (token == NULL || token->text == NULL)
        return;

    len = strlen(token->text);

    snprintf(feature_name, sizeof(feature_name), "length=%d", len);
    add_feature(features, feature_name, 1.0);

    if (len == 1)
    {
        add_feature(features, "length_1", 1.0);
    }
    else if (len >= 2 && len <= 4)
    {
        add_feature(features, "length_2_4", 1.0);
    }
    else if (len >= 5 && len <= 8)
    {
        add_feature(features, "length_5_8", 1.0);
    }
    else if (len > 8)
    {
        add_feature(features, "length_long", 1.0);
    }
}

/*
 * Extract character-based features
 */
void extract_character_features(TokenInfo *token, FeatureSet *features)
{
    if (token == NULL || token->text == NULL)
        return;

    if (has_digit(token->text))
    {
        add_feature(features, "has_digit", 1.0);
    }

    if (has_punctuation(token->text))
    {
        add_feature(features, "has_punctuation", 1.0);
    }

    if (is_numeric(token->text))
    {
        add_feature(features, "is_numeric", 1.0);
    }
}

/*
 * Extract context features from neighboring tokens
 */
void extract_context_features(TokenInfo *tokens, int num_tokens, int position, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];
    int window = FEATURE_WINDOW_SIZE;

    if (tokens == NULL || position < 0 || position >= num_tokens)
        return;

    /* Previous tokens */
    for (int i = 1; i <= window; i++)
    {
        int prev_pos = position - i;
        if (prev_pos >= 0)
        {
            snprintf(feature_name, sizeof(feature_name), "prev_%d=%s", i, tokens[prev_pos].text);
            add_feature(features, feature_name, 0.8);
        }
        else
        {
            snprintf(feature_name, sizeof(feature_name), "prev_%d=BOS", i);
            add_feature(features, feature_name, 0.5);
        }
    }

    /* Next tokens */
    for (int i = 1; i <= window; i++)
    {
        int next_pos = position + i;
        if (next_pos < num_tokens)
        {
            snprintf(feature_name, sizeof(feature_name), "next_%d=%s", i, tokens[next_pos].text);
            add_feature(features, feature_name, 0.8);
        }
        else
        {
            snprintf(feature_name, sizeof(feature_name), "next_%d=EOS", i);
            add_feature(features, feature_name, 0.5);
        }
    }
}

/*
 * Extract position features
 */
void extract_position_features(TokenInfo *token, int total_tokens, FeatureSet *features)
{
    char feature_name[MAX_FEATURE_NAME_LEN];
    float relative_pos;

    if (token == NULL)
        return;

    if (token->is_first)
    {
        add_feature(features, "is_first", 1.0);
    }

    if (token->is_last)
    {
        add_feature(features, "is_last", 1.0);
    }

    /* Relative position */
    if (total_tokens > 1)
    {
        relative_pos = (float)token->position / (total_tokens - 1);
        if (relative_pos < 0.33)
        {
            add_feature(features, "pos_early", 1.0);
        }
        else if (relative_pos < 0.67)
        {
            add_feature(features, "pos_middle", 1.0);
        }
        else
        {
            add_feature(features, "pos_late", 1.0);
        }
    }

    snprintf(feature_name, sizeof(feature_name), "position=%d", token->position);
    add_feature(features, feature_name, 0.5);
}

/*
 * Create CRFSuite instance from token sequence
 */
crfsuite_instance_t *create_crf_instance_from_tokens(TokenInfo *tokens, int num_tokens)
{
    crfsuite_instance_t *instance;
    crfsuite_item_t item;
    crfsuite_attribute_t attr;
    FeatureSet *features;
    int ret;

    if (tokens == NULL || num_tokens <= 0)
        return NULL;

    instance = (crfsuite_instance_t *)palloc0(sizeof(crfsuite_instance_t));
    ret = crfsuite_instance_init(instance);
    if (ret != CRFSUITE_SUCCESS)
    {
        pfree(instance);
        return NULL;
    }

    /* Extract features for each token */
    for (int i = 0; i < num_tokens; i++)
    {
        ret = crfsuite_item_init(&item);
        if (ret != CRFSUITE_SUCCESS)
        {
            crfsuite_instance_finish(instance);
            pfree(instance);
            return NULL;
        }

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

        /* Add features to CRFSuite item */
        for (int j = 0; j < features->num_features; j++)
        {
            ret = crfsuite_attribute_init(&attr);
            if (ret == CRFSUITE_SUCCESS)
            {
                ret = crfsuite_attribute_set(&attr, features->features[j].name,
                                             features->features[j].weight);
                if (ret == CRFSUITE_SUCCESS)
                {
                    crfsuite_item_append(&item, &attr);
                }
                crfsuite_attribute_finish(&attr);
            }
        }

        crfsuite_instance_append(instance, &item);
        crfsuite_item_finish(&item);
        free_feature_set(features);
    }

    return instance;
}

/*
 * Free CRFSuite instance
 */
void free_crf_instance(crfsuite_instance_t *instance)
{
    if (instance == NULL)
        return;

    crfsuite_instance_finish(instance);
    pfree(instance);
}

/* Utility functions */

/*
 * Get token shape (similar to Stanford NER)
 */
char *get_token_shape(const char *token)
{
    if (token == NULL)
        return NULL;

    int len = strlen(token);
    char *shape = (char *)palloc((len + 1) * sizeof(char));

    for (int i = 0; i < len; i++)
    {
        if (isupper(token[i]))
        {
            shape[i] = 'X';
        }
        else if (islower(token[i]))
        {
            shape[i] = 'x';
        }
        else if (isdigit(token[i]))
        {
            shape[i] = 'd';
        }
        else
        {
            shape[i] = token[i]; /* Keep punctuation as is */
        }
    }
    shape[len] = '\0';

    return shape;
}

/*
 * Check if token is capitalized
 */
bool is_capitalized(const char *token)
{
    return token != NULL && strlen(token) > 0 && isupper(token[0]);
}

/*
 * Check if token is all uppercase
 */
bool is_all_caps(const char *token)
{
    if (token == NULL || strlen(token) == 0)
        return false;

    for (const char *p = token; *p; p++)
    {
        if (isalpha(*p) && !isupper(*p))
        {
            return false;
        }
    }
    return true;
}

/*
 * Check if token is all lowercase
 */
bool is_all_lower(const char *token)
{
    if (token == NULL || strlen(token) == 0)
        return false;

    for (const char *p = token; *p; p++)
    {
        if (isalpha(*p) && !islower(*p))
        {
            return false;
        }
    }
    return true;
}

/*
 * Check if token contains digits
 */
bool has_digit(const char *token)
{
    if (token == NULL)
        return false;

    for (const char *p = token; *p; p++)
    {
        if (isdigit(*p))
        {
            return true;
        }
    }
    return false;
}

/*
 * Check if token contains punctuation
 */
bool has_punctuation(const char *token)
{
    if (token == NULL)
        return false;

    for (const char *p = token; *p; p++)
    {
        if (ispunct(*p))
        {
            return true;
        }
    }
    return false;
}

/*
 * Check if token is numeric
 */
bool is_numeric(const char *token)
{
    if (token == NULL || strlen(token) == 0)
        return false;

    for (const char *p = token; *p; p++)
    {
        if (!isdigit(*p) && *p != '.' && *p != ',')
        {
            return false;
        }
    }
    return true;
}

/*
 * Get prefix of specified length
 */
char *get_prefix(const char *token, int length)
{
    if (token == NULL || length <= 0)
        return NULL;

    int token_len = strlen(token);
    if (length > token_len)
        length = token_len;

    char *prefix = (char *)palloc((length + 1) * sizeof(char));
    strncpy(prefix, token, length);
    prefix[length] = '\0';

    return prefix;
}

/*
 * Get suffix of specified length
 */
char *get_suffix(const char *token, int length)
{
    if (token == NULL || length <= 0)
        return NULL;

    int token_len = strlen(token);
    if (length > token_len)
        length = token_len;

    char *suffix = (char *)palloc((length + 1) * sizeof(char));
    strcpy(suffix, token + token_len - length);

    return suffix;
}