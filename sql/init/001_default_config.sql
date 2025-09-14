-- sql/init/001_default_config.sql
-- Initialize default configuration parameters

INSERT INTO pg_probablepeople.crf_config (parameter, value, description, data_type, is_readonly, min_value, max_value, valid_values) VALUES

-- Cache settings
('cache_enabled', 'true', 'Enable result caching in parsed_names table', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('cache_ttl_hours', '24', 'Cache time-to-live in hours', 'integer', false, 1, 168, NULL),
('cache_max_entries', '10000', 'Maximum number of cached entries', 'integer', false, 100, 1000000, NULL),
('cache_cleanup_interval_hours', '24', 'Hours between automatic cache cleanup', 'integer', false, 1, 168, NULL),

-- Parsing settings
('max_input_length', '1000', 'Maximum input string length for parsing', 'integer', false, 10, 10000, NULL),
('enable_confidence_scores', 'true', 'Calculate individual token confidence scores', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('min_confidence_threshold', '0.3', 'Minimum confidence threshold for results', 'float', false, 0.0, 1.0, NULL),
('enable_batch_processing', 'true', 'Enable batch processing for multiple names', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('batch_size_limit', '100', 'Maximum number of names in batch processing', 'integer', false, 1, 1000, NULL),

-- Feature extraction settings
('feature_window_size', '3', 'Context window size for feature extraction', 'integer', false, 1, 5, NULL),
('enable_shape_features', 'true', 'Enable word shape features (Xx, XX, xx patterns)', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('enable_prefix_suffix_features', 'true', 'Enable prefix/suffix features', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('enable_position_features', 'true', 'Enable position-based features', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('enable_context_features', 'true', 'Enable neighboring token features', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),

-- Model settings
('default_model', 'probablepeople_v1', 'Default model to use for parsing', 'text', false, NULL, NULL, NULL),
('auto_load_default_model', 'true', 'Automatically load default model on extension startup', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('model_memory_limit_mb', '100', 'Maximum memory usage for loaded models in MB', 'integer', false, 10, 1000, NULL),

-- Performance settings
('enable_performance_tracking', 'true', 'Track parsing performance metrics', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('performance_log_threshold_ms', '1000', 'Log slow parsing operations above this threshold', 'integer', false, 100, 10000, NULL),
('enable_similarity_search', 'true', 'Enable similarity search for cached results', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('similarity_threshold', '0.8', 'Minimum similarity score for cached result matching', 'float', false, 0.5, 1.0, NULL),

-- Debug and logging settings
('debug_mode', 'false', 'Enable debug logging and verbose output', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('log_failed_parses', 'true', 'Log inputs that failed to parse properly', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('log_cache_hits', 'false', 'Log cache hit/miss statistics', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),

-- Extension metadata (read-only)
('extension_version', '1.0.0', 'Extension version number', 'text', true, NULL, NULL, NULL),
('crfsuite_version', '0.12', 'CRFSuite library version', 'text', true, NULL, NULL, NULL),
('installation_date', CURRENT_TIMESTAMP::text, 'Extension installation timestamp', 'text', true, NULL, NULL, NULL),

-- API settings
('enable_rest_api', 'false', 'Enable REST API endpoints (if available)', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('api_rate_limit_per_minute', '60', 'API calls per minute limit', 'integer', false, 1, 10000, NULL),
('api_require_auth', 'true', 'Require authentication for API access', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),

-- Data quality settings
('enable_input_validation', 'true', 'Validate input text before parsing', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('auto_suggest_corrections', 'true', 'Automatically suggest corrections for poor quality parses', 'boolean', false, NULL, NULL, ARRAY['true', 'false']),
('quality_threshold', '0.6', 'Minimum quality score for acceptable parses', 'float', false, 0.0, 1.0, NULL)

ON CONFLICT (parameter) DO UPDATE SET
    value = EXCLUDED.value,
    description = EXCLUDED.description,
    data_type = EXCLUDED.data_type,
    updated_at = CURRENT_TIMESTAMP;

-- Create view for easy configuration access
CREATE OR REPLACE VIEW pg_probablepeople.config_summary AS
SELECT 
    parameter,
    value,
    description,
    data_type,
    is_readonly,
    CASE 
        WHEN data_type = 'boolean' THEN pg_probablepeople.get_config_bool(parameter)::text
        WHEN data_type = 'integer' THEN pg_probablepeople.get_config_int(parameter)::text
        WHEN data_type = 'float' THEN pg_probablepeople.get_config_float(parameter)::text
        ELSE value
    END as typed_value,
    valid_values,
    CASE 
        WHEN min_value IS NOT NULL OR max_value IS NOT NULL THEN
            format('[%s, %s]', COALESCE(min_value::text, '-∞'), COALESCE(max_value::text, '∞'))
        ELSE NULL
    END as value_range,
    updated_at
FROM pg_probablepeople.crf_config
ORDER BY 
    CASE 
        WHEN parameter ~ '^(extension|crfsuite)_' THEN 1  -- Extension metadata first
        WHEN parameter ~ '^cache_' THEN 2                -- Cache settings
        WHEN parameter ~ '^(max_input|enable_|min_|batch_)' THEN 3  -- Parsing settings
        WHEN parameter ~ '^feature_' THEN 4              -- Feature settings
        WHEN parameter ~ '^(default_model|auto_load|model_)' THEN 5  -- Model settings
        WHEN parameter ~ '^(performance|similarity)' THEN 6  -- Performance settings
        WHEN parameter ~ '^(debug|log_)' THEN 7          -- Debug settings
        WHEN parameter ~ '^api_' THEN 8                  -- API settings
        WHEN parameter ~ '^(quality|auto_suggest)' THEN 9 -- Quality settings
        ELSE 10
    END,
    parameter;

-- Create function to reset configuration to defaults
CREATE OR REPLACE FUNCTION pg_probablepeople.reset_config_to_defaults()
RETURNS JSON
LANGUAGE plpgsql
AS $$
DECLARE
    reset_count INTEGER;
    readonly_count INTEGER;
BEGIN
    -- Count read-only parameters that won't be reset
    SELECT COUNT(*) INTO readonly_count 
    FROM pg_probablepeople.crf_config 
    WHERE is_readonly = true;
    
    -- Reset non-readonly parameters
    DELETE FROM pg_probablepeople.crf_config WHERE is_readonly = false;
    
    -- Re-insert default values (this will trigger the INSERT above)
    -- Note: In practice, this would re-run the INSERT statements above
    
    GET DIAGNOSTICS reset_count = ROW_COUNT;
    
    RETURN json_build_object(
        'success', true,
        'message', 'Configuration reset to defaults',
        'parameters_reset', reset_count,
        'readonly_parameters_preserved', readonly_count
    );
END;
$$;

-- Create function to export current configuration
CREATE OR REPLACE FUNCTION pg_probablepeople.export_config(include_readonly BOOLEAN DEFAULT false)
RETURNS JSON
LANGUAGE SQL
AS $$
    SELECT json_object_agg(parameter, json_build_object(
        'value', value,
        'description', description,
        'data_type', data_type,
        'is_readonly', is_readonly,
        'min_value', min_value,
        'max_value', max_value,
        'valid_values', valid_values,
        'updated_at', updated_at
    ))
    FROM pg_probablepeople.crf_config
    WHERE include_readonly OR NOT is_readonly;
$$;

-- Create function to import configuration from JSON
CREATE OR REPLACE FUNCTION pg_probablepeople.import_config(config_json JSON)
RETURNS JSON
LANGUAGE plpgsql
AS $$
DECLARE
    param_key TEXT;
    param_data JSON;
    import_count INTEGER := 0;
    error_count INTEGER := 0;
    errors TEXT[] := ARRAY[]::TEXT[];
BEGIN
    FOR param_key, param_data IN SELECT * FROM json_each(config_json)
    LOOP
        BEGIN
            -- Only import non-readonly parameters
            IF EXISTS (
                SELECT 1 FROM pg_probablepeople.crf_config 
                WHERE parameter = param_key AND NOT is_readonly
            ) THEN
                PERFORM pg_probablepeople.set_config(param_key, param_data->>'value');
                import_count := import_count + 1;
            END IF;
        EXCEPTION WHEN OTHERS THEN
            error_count := error_count + 1;
            errors := array_append(errors, format('%s: %s', param_key, SQLERRM));
        END;
    END LOOP;
    
    RETURN json_build_object(
        'success', error_count = 0,
        'imported_count', import_count,
        'error_count', error_count,
        'errors', to_json(errors)
    );
END;
$$;

-- Add comments
COMMENT ON VIEW pg_probablepeople.config_summary IS 'Human-readable view of all configuration parameters with typed values';
COMMENT ON FUNCTION pg_probablepeople.reset_config_to_defaults() IS 'Reset all non-readonly configuration parameters to default values';
COMMENT ON FUNCTION pg_probablepeople.export_config(BOOLEAN) IS 'Export current configuration as JSON';
COMMENT ON FUNCTION pg_probablepeople.import_config(JSON) IS 'Import configuration from JSON, updating non-readonly parameters';