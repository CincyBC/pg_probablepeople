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


CREATE TYPE parsed_name AS (
  prefix text,
  given_name text,
  middle_name text,
  surname text,
  suffix text,
  nickname text,
  corporation_name text,
  corporation_type text,
  organization text,
  other text
);

CREATE FUNCTION parse_name_cols(input_text text)
RETURNS parsed_name
AS '$libdir/pg_probablepeople', 'parse_name_cols'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION parse_name_cols(text) IS 'Parse a name into standardized columns';
