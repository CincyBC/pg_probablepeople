/* pg_probablepeople--1.0.0.sql */

CREATE OR REPLACE FUNCTION parse_name(input_text text)
RETURNS TABLE(token text, label text)
AS '$libdir/pg_probablepeople', 'parse_name_crf'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION parse_name(text) IS 'Parse a name into its components using a CRF model';

CREATE OR REPLACE FUNCTION tag_name(input_text text)
RETURNS jsonb
AS '$libdir/pg_probablepeople', 'tag_name_crf'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION tag_name(text) IS 'Tag a name with its components using a CRF model';

