#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "feature_extractor.h"
#include "postgres.h"

// Test for get_token_shape
void test_get_token_shape() {
    printf("Running test_get_token_shape...\\n");
    char *shape = get_token_shape("John-Doe's");
    assert(strcmp(shape, "Xxxx-Xxx'x") == 0);
    pfree(shape);

    shape = get_token_shape("123-456");
    assert(strcmp(shape, "ddd-ddd") == 0);
    pfree(shape);

    shape = get_token_shape("Mixed123");
    assert(strcmp(shape, "Xxxxxddd") == 0);
    pfree(shape);
    printf("test_get_token_shape passed.\\n");
}

// Test for is_capitalized
void test_is_capitalized() {
    printf("Running test_is_capitalized...\\n");
    assert(is_capitalized("John"));
    assert(!is_capitalized("john"));
    assert(!is_capitalized(""));
    assert(!is_capitalized(NULL));
    printf("test_is_capitalized passed.\\n");
}

// Test for get_prefix
void test_get_prefix() {
    printf("Running test_get_prefix...\\n");
    char *prefix = get_prefix("testing", 3);
    assert(strcmp(prefix, "tes") == 0);
    pfree(prefix);

    prefix = get_prefix("testing", 10);
    assert(strcmp(prefix, "testing") == 0);
    pfree(prefix);

    prefix = get_prefix("a", 2);
    assert(strcmp(prefix, "a") == 0);
    pfree(prefix);
    printf("test_get_prefix passed.\\n");
}

int main() {
    test_get_token_shape();
    test_is_capitalized();
    test_get_prefix();

    printf("All feature_extractor tests passed!\\n");
    return 0;
}
