-- Regression tests for pg_probablepeople

CREATE EXTENSION pg_probablepeople;

-- Test 1: Tag a simple name (JSONB output)
SELECT tag_name('John Doe') ? 'tokens' AS has_tokens;
SELECT tag_name('John Doe') ? 'model_version' AS has_version;

-- Test 2: Parse a simple name (Table output)
SELECT count(*) FROM parse_name('John Doe');
-- Should be at least 2 tokens (John, Doe)

-- Test 3: Parse with prefix
SELECT token, label FROM parse_name('Mr. John Doe');

-- Test 4: Error handling (NULL input)
SELECT tag_name(NULL);
SELECT * FROM parse_name(NULL);

-- Test 5: Parse with suffix
SELECT token, label FROM parse_name('John Doe Esq.');

-- Test 6: Parse with generational suffix
SELECT token, label FROM parse_name('John Doe III');

-- Test 7: Parse with prefix and suffix
SELECT token, label FROM parse_name('Dr. Hugh F Smission Jr.')

-- Test 8: Parse name with title
SELECT token, label FROM parse_name('President Joe Biden');

-- Clean up
DROP EXTENSION pg_probablepeople;
