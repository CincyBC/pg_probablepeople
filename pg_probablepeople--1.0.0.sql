/* Schema and tables for CRF Name NER */
CREATE SCHEMA IF NOT EXISTS crfname_ner;

CREATE TABLE crfname_ner.crf_config (
    parameter text PRIMARY KEY,
    value text
);

INSERT INTO crfname_ner.crf_config (parameter, value) VALUES ('cache_enabled', 'false');

CREATE TABLE crfname_ner.crf_models (
    name text PRIMARY KEY,
    version text,
    is_active boolean DEFAULT false,
    model_data bytea
);

CREATE TABLE crfname_ner.parsed_names (
    original_text text PRIMARY KEY,
    parsed_components jsonb,
    confidence_scores jsonb,
    model_version text,
    processing_time_ms integer,
    created_at timestamptz DEFAULT now()
);

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
