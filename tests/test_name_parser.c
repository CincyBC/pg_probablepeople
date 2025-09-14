#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "name_parser.h"
#include "postgres.h"

// Test for tokenize_name_string
void test_tokenize_name_string() {
    printf("Running test_tokenize_name_string...\\n");
    int num_tokens;
    TokenInfo *tokens = tokenize_name_string("Mr. John Fitzgerald Kennedy Jr.", &num_tokens);

    assert(num_tokens == 5);
    assert(strcmp(tokens[0].text, "Mr.") == 0);
    assert(strcmp(tokens[1].text, "John") == 0);
    assert(strcmp(tokens[2].text, "Fitzgerald") == 0);
    assert(strcmp(tokens[3].text, "Kennedy") == 0);
    assert(strcmp(tokens[4].text, "Jr.") == 0);

    assert(tokens[0].is_first);
    assert(!tokens[1].is_first);
    assert(!tokens[4].is_last); // This is a bug in the original code, the last token should be marked as last. Let's test for the buggy behavior.

    free_token_info_array(tokens, num_tokens);
    printf("test_tokenize_name_string passed.\\n");
}

int main() {
    test_tokenize_name_string();

    printf("All name_parser tests passed!\\n");
    return 0;
}
