-- sql/functions/001_parse_name.sql
-- Core name parsing functions exposed to SQL

-- Main parsing function that returns a table of tokens and labels
CREATE OR REPLACE FUNCTION pg_probablepeople.parse_name(input_text TEXT)
RETURNS TABLE(
    token TEXT,
    label TEXT,
    confidence FLOAT,
    position INTEGER
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'parse_name_crf';

-- Alternative parsing function that returns structured JSON
CREATE OR REPLACE FUNCTION pg_probablepeople.tag_name(input_text TEXT)
RETURNS JSON
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'tag_name_crf';

-- Batch parsing function for multiple names
CREATE OR REPLACE FUNCTION pg_probablepeople.parse_names_batch(input_texts TEXT[])
RETURNS TABLE(
    input_index INTEGER,
    original_text TEXT,
    token TEXT,
    label TEXT,
    confidence FLOAT,
    position INTEGER
)
LANGUAGE plpgsql
AS $$
DECLARE
    text_item TEXT;
    text_index INTEGER := 1;
    parse_result RECORD;
BEGIN
    FOREACH text_item IN ARRAY input_texts
    LOOP
        FOR parse_result IN 
            SELECT token, label, confidence, position 
            FROM pg_probablepeople.parse_name(text_item)
        LOOP
            input_index := text_index;
            original_text := text_item;
            token := parse_result.token;
            label := parse_result.label;
            confidence := parse_result.confidence;
            position := parse_result.position;
            RETURN NEXT;
        END LOOP;
        
        text_index := text_index + 1;
    END LOOP;
END;
$$;

-- Function to parse names and return probablepeople-style output
CREATE OR REPLACE FUNCTION pg_probablepeople.parse_name_structured(input_text TEXT)
RETURNS JSON
LANGUAGE plpgsql
AS $$
DECLARE
    result JSON;
    components JSON := '{}';
    parse_result RECORD;
    confidence_total FLOAT := 0.0;
    token_count INTEGER := 0;
    name_type TEXT := 'Person';
BEGIN
    -- Check cache first
    SELECT parsed_components INTO result
    FROM pg_probablepeople.parsed_names 
    WHERE original_text = input_text
    AND created_at > NOW() - INTERVAL '24 hours'
    LIMIT 1;
    
    IF result IS NOT NULL THEN
        -- Update access statistics
        UPDATE pg_probablepeople.parsed_names 
        SET last_accessed = CURRENT_TIMESTAMP,
            access_count = access_count + 1
        WHERE original_text = input_text;
        
        RETURN result;
    END IF;
    
    -- Parse the name using C extension
    FOR parse_result IN 
        SELECT token, label, confidence, position 
        FROM pg_probablepeople.parse_name(input_text)
        ORDER BY position
    LOOP
        -- Build components JSON
        IF components ? parse_result.label THEN
            -- Append to existing label
            components := jsonb_set(
                components::jsonb,
                ARRAY[parse_result.label],
                to_jsonb(
                    (components->>parse_result.label) || ' ' || parse_result.token
                )
            )::json;
        ELSE
            -- Create new label
            components := jsonb_set(
                components::jsonb,
                ARRAY[parse_result.label],
                to_jsonb(parse_result.token)
            )::json;
        END IF;
        
        confidence_total := confidence_total + COALESCE(parse_result.confidence, 0.0);
        token_count := token_count + 1;
    END LOOP;
    
    -- Determine if this looks like a corporation
    IF components ? 'CorporationName' OR components ? 'CorporationLegalType' THEN
        name_type := 'Corporation';
    END IF;
    
    -- Build final result
    result := json_build_object(
        'components', components,
        'type', name_type,
        'confidence', CASE WHEN token_count > 0 THEN confidence_total / token_count ELSE 0.0 END,
        'processed_at', CURRENT_TIMESTAMP
    );
    
    RETURN result;
END;
$$;

-- Function to extract only specific name components
CREATE OR REPLACE FUNCTION pg_probablepeople.extract_name_component(
    input_text TEXT, 
    component_type TEXT
)
RETURNS TEXT
LANGUAGE plpgsql
AS $$
DECLARE
    result_text TEXT := '';
    parse_result RECORD;
BEGIN
    FOR parse_result IN 
        SELECT token 
        FROM pg_probablepeople.parse_name(input_text)
        WHERE label = component_type
        ORDER BY position
    LOOP
        IF result_text = '' THEN
            result_text := parse_result.token;
        ELSE
            result_text := result_text || ' ' || parse_result.token;
        END IF;
    END LOOP;
    
    RETURN NULLIF(result_text, '');
END;
$$;

-- Convenience functions for common name components
CREATE OR REPLACE FUNCTION pg_probablepeople.extract_first_name(input_text TEXT)
RETURNS TEXT
LANGUAGE SQL
AS $$
    SELECT pg_probablepeople.extract_name_component(input_text, 'GivenName');
$$;

CREATE OR REPLACE FUNCTION pg_probablepeople.extract_last_name(input_text TEXT)
RETURNS TEXT
LANGUAGE SQL
AS $$
    SELECT pg_probablepeople.extract_name_component(input_text, 'Surname');
$$;

CREATE OR REPLACE FUNCTION pg_probablepeople.extract_middle_name(input_text TEXT)
RETURNS TEXT
LANGUAGE SQL
AS $$
    SELECT pg_probablepeople.extract_name_component(input_text, 'MiddleName');
$$;

CREATE OR REPLACE FUNCTION pg_probablepeople.extract_prefix(input_text TEXT)
RETURNS TEXT
LANGUAGE SQL
AS $$
    SELECT pg_probablepeople.extract_name_component(input_text, 'PrefixMarital');
$$;

CREATE OR REPLACE FUNCTION pg_probablepeople.extract_suffix(input_text TEXT)
RETURNS TEXT
LANGUAGE SQL
AS $$
    SELECT pg_probablepeople.extract_name_component(input_text, 'SuffixGenerational');
$$;

-- Function to validate parsing quality
CREATE OR REPLACE FUNCTION pg_probablepeople.validate_parse_quality(input_text TEXT)
RETURNS JSON
LANGUAGE plpgsql
AS $$
DECLARE
    result JSON;
    token_count INTEGER;
    labeled_count INTEGER;
    unlabeled_count INTEGER;
    avg_confidence FLOAT;
    has_surname BOOLEAN := FALSE;
    has_given_name BOOLEAN := FALSE;
    quality_score FLOAT;
    issues TEXT[] := ARRAY[]::TEXT[];
BEGIN
    -- Get parsing statistics
    SELECT 
        COUNT(*) as total_tokens,
        COUNT(*) FILTER (WHERE label != 'Unknown') as labeled_tokens,
        COUNT(*) FILTER (WHERE label = 'Unknown') as unlabeled_tokens,
        AVG(COALESCE(confidence, 0.0)) as avg_conf,
        bool_or(label = 'Surname') as has_surname,
        bool_or(label = 'GivenName') as has_given_name
    INTO token_count, labeled_count, unlabeled_count, avg_confidence, has_surname, has_given_name
    FROM pg_probablepeople.parse_name(input_text);
    
    -- Calculate quality score (0-1)
    quality_score := CASE 
        WHEN token_count = 0 THEN 0.0
        ELSE (labeled_count::FLOAT / token_count::FLOAT) * COALESCE(avg_confidence, 0.0)
    END;
    
    -- Identify potential issues
    IF NOT has_surname AND NOT has_given_name THEN
        issues := array_append(issues, 'No recognizable name components found');
    END IF;
    
    IF unlabeled_count > 0 THEN
        issues := array_append(issues, format('%s unlabeled tokens', unlabeled_count));
    END IF;
    
    IF avg_confidence < 0.5 THEN
        issues := array_append(issues, 'Low average confidence');
    END IF;
    
    IF token_count > 10 THEN
        issues := array_append(issues, 'Unusually long input - may not be a name');
    END IF;
    
    result := json_build_object(
        'quality_score', quality_score,
        'token_count', token_count,
        'labeled_count', labeled_count,
        'unlabeled_count', unlabeled_count,
        'average_confidence', avg_confidence,
        'has_surname', has_surname,
        'has_given_name', has_given_name,
        'issues', to_json(issues),
        'recommendation', CASE 
            WHEN quality_score >= 0.8 THEN 'High quality parse'
            WHEN quality_score >= 0.6 THEN 'Good quality parse'
            WHEN quality_score >= 0.4 THEN 'Moderate quality - review recommended'
            ELSE 'Low quality - manual review required'
        END
    );
    
    RETURN result;
END;
$$;

-- Function to suggest corrections based on common patterns
CREATE OR REPLACE FUNCTION pg_probablepeople.suggest_name_corrections(input_text TEXT)
RETURNS TABLE(
    suggestion TEXT,
    confidence FLOAT,
    reason TEXT
)
LANGUAGE plpgsql
AS $$
DECLARE
    original_parse JSON;
    cleaned_text TEXT;
    words TEXT[];
    word TEXT;
    suggestions_count INTEGER := 0;
BEGIN
    -- Get original parse quality
    original_parse := pg_probablepeople.validate_parse_quality(input_text);
    
    -- If quality is already high, no suggestions needed
    IF (original_parse->>'quality_score')::FLOAT >= 0.8 THEN
        RETURN;
    END IF;
    
    -- Try cleaning up common issues
    cleaned_text := input_text;
    
    -- Remove extra spaces
    cleaned_text := regexp_replace(cleaned_text, '\s+', ' ', 'g');
    
    -- Remove leading/trailing spaces
    cleaned_text := trim(cleaned_text);
    
    -- Suggest capitalizing first letters if all lowercase
    IF input_text = lower(input_text) AND input_text != upper(input_text) THEN
        suggestion := initcap(cleaned_text);
        confidence := 0.8;
        reason := 'Capitalized first letters of words';
        suggestions_count := suggestions_count + 1;
        RETURN NEXT;
    END IF;
    
    -- Suggest removing excessive punctuation
    IF cleaned_text ~ '[!@#$%^&*()_+=\[\]{}|;'':"",.<>?/]' THEN
        suggestion := regexp_replace(cleaned_text, '[!@#$%^&*()_+=\[\]{}|;''"",<>?/]', '', 'g');
        suggestion := regexp_replace(suggestion, '\s+', ' ', 'g');
        suggestion := trim(suggestion);
        confidence := 0.7;
        reason := 'Removed excessive punctuation';
        suggestions_count := suggestions_count + 1;
        RETURN NEXT;
    END IF;
    
    -- If we have too many words, suggest keeping only the first few
    words := string_to_array(cleaned_text, ' ');
    IF array_length(words, 1) > 6 THEN
        suggestion := array_to_string(words[1:4], ' ');
        confidence := 0.6;
        reason := 'Truncated to first 4 words - original may not be a name';
        suggestions_count := suggestions_count + 1;
        RETURN NEXT;
    END IF;
    
    -- If no suggestions generated, indicate that manual review is needed
    IF suggestions_count = 0 THEN
        suggestion := input_text;
        confidence := 0.3;
        reason := 'No automatic corrections available - manual review recommended';
        RETURN NEXT;
    END IF;
END;
$$;

-- Add function comments
COMMENT ON FUNCTION pg_probablepeople.parse_name(TEXT) IS 'Parse name into tokens and labels using CRF model';
COMMENT ON FUNCTION pg_probablepeople.tag_name(TEXT) IS 'Parse name and return JSON structure similar to probablepeople';
COMMENT ON FUNCTION pg_probablepeople.parse_names_batch(TEXT[]) IS 'Parse multiple names in a single call';
COMMENT ON FUNCTION pg_probablepeople.parse_name_structured(TEXT) IS 'Parse name with caching and structured output';
COMMENT ON FUNCTION pg_probablepeople.extract_name_component(TEXT, TEXT) IS 'Extract specific component type from parsed name';
COMMENT ON FUNCTION pg_probablepeople.validate_parse_quality(TEXT) IS 'Validate quality of name parsing result';
COMMENT ON FUNCTION pg_probablepeople.suggest_name_corrections(TEXT) IS 'Suggest corrections for poorly parsed names';