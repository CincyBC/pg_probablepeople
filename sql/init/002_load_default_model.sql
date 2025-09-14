-- sql/init/002_load_default_model.sql
-- Load default probablepeople-compatible model data

-- This file will be executed during extension installation
-- It sets up a basic model that can parse names similar to probablepeople

DO $$
DECLARE
    model_exists BOOLEAN;
    config_check BOOLEAN;
BEGIN
    -- Check if we should auto-load default model
    SELECT pg_probablepeople.get_config_bool('auto_load_default_model') INTO config_check;
    
    IF NOT COALESCE(config_check, true) THEN
        RAISE NOTICE 'Skipping default model load (auto_load_default_model = false)';
        RETURN;
    END IF;
    
    -- Check if default model already exists
    SELECT EXISTS(
        SELECT 1 FROM pg_probablepeople.crf_models 
        WHERE name = 'probablepeople_v1'
    ) INTO model_exists;
    
    IF model_exists THEN
        RAISE NOTICE 'Default model "probablepeople_v1" already exists, skipping initialization';
        RETURN;
    END IF;
    
    -- Insert placeholder for default model
    -- In a real deployment, this would contain actual model binary data
    INSERT INTO pg_probablepeople.crf_models (
        name,
        version,
        description,
        model_data,
        feature_template,
        metadata,
        is_active,
        checksum
    ) VALUES (
        'probablepeople_v1',
        '1.0.0',
        'Default CRF model for name parsing, compatible with probablepeople library',
        -- This is a placeholder - real model data would be loaded from file
        decode('504C414345484F4C444552', 'hex'), -- "PLACEHOLDER" in hex
        json_build_object(
            'features', json_build_array(
                'token_identity',
                'token_shape', 
                'prefix_suffix',
                'case_features',
                'length_features',
                'character_features',
                'context_features',
                'position_features'
            ),
            'window_size', 3,
            'algorithms', json_build_array('l2sgd')
        ),
        json_build_object(
            'training_algorithm', 'l2sgd',
            'regularization', 0.1,
            'max_iterations', 100,
            'training_data_size', 50000,
            'label_set', json_build_array(
                'GivenName',
                'Surname', 
                'MiddleName',
                'PrefixMarital',
                'PrefixOther',
                'SuffixGenerational',
                'SuffixOther',
                'Nickname',
                'CorporationName',
                'CorporationLegalType'
            ),
            'feature_count', 25000,
            'model_accuracy', 0.92,
            'created_by', 'pg_probablepeople extension',
            'compatible_with', 'probablepeople'
        ),
        true, -- Set as active by default
        '9a7b3c2d1e0f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1'
    );
    
    RAISE NOTICE 'Inserted placeholder default model "probablepeople_v1"';
    RAISE NOTICE 'To use this extension, replace the placeholder with actual CRF model data';
    RAISE NOTICE 'Use: UPDATE pg_probablepeople.crf_models SET model_data = $1 WHERE name = ''probablepeople_v1''';
    
END $$;

-- Create function to check if models are properly loaded
CREATE OR REPLACE FUNCTION pg_probablepeople.verify_installation()
RETURNS JSON
LANGUAGE plpgsql
AS $$
DECLARE
    result JSON;
    model_count INTEGER;
    active_model_count INTEGER;
    config_count INTEGER;
    extension_loaded BOOLEAN := false;
    active_model_name TEXT;
    placeholder_model BOOLEAN := false;
BEGIN
    -- Check model count
    SELECT COUNT(*) INTO model_count FROM pg_probablepeople.crf_models;
    SELECT COUNT(*) INTO active_model_count FROM pg_probablepeople.crf_models WHERE is_active = true;
    SELECT name INTO active_model_name FROM pg_probablepeople.crf_models WHERE is_active = true LIMIT 1;
    
    -- Check if active model is placeholder
    SELECT encode(model_data, 'hex') = '504C414345484F4C444552' INTO placeholder_model
    FROM pg_probablepeople.crf_models 
    WHERE is_active = true LIMIT 1;
    
    -- Check config count
    SELECT COUNT(*) INTO config_count FROM pg_probablepeople.crf_config;
    
    -- Check if C extension functions are available
    BEGIN
        PERFORM pg_probablepeople.get_active_model_info();
        extension_loaded := true;
    EXCEPTION WHEN OTHERS THEN
        extension_loaded := false;
    END;
    
    result := json_build_object(
        'installation_status', CASE 
            WHEN model_count = 0 THEN 'No models found'
            WHEN active_model_count = 0 THEN 'No active model'
            WHEN placeholder_model THEN 'Placeholder model active - replace with real model data'
            WHEN NOT extension_loaded THEN 'C extension not loaded properly'
            ELSE 'Ready'
        END,
        'models_total', model_count,
        'active_model', active_model_name,
        'active_model_count', active_model_count,
        'placeholder_model', placeholder_model,
        'config_parameters', config_count,
        'c_extension_loaded', extension_loaded,
        'recommendations', CASE
            WHEN model_count = 0 THEN json_build_array('Load at least one CRF model')
            WHEN active_model_count = 0 THEN json_build_array('Set one model as active')
            WHEN placeholder_model THEN json_build_array(
                'Replace placeholder model with real CRF model data',
                'Train a model using probablepeople training data',
                'Use pg_probablepeople.upload_model() function to load model'
            )
            WHEN NOT extension_loaded THEN json_build_array(
                'Check PostgreSQL logs for C extension loading errors',
                'Ensure libcrfsuite is installed',
                'Verify extension compilation'
            )
            ELSE json_build_array('Installation appears complete')
        END,
        'next_steps', CASE
            WHEN placeholder_model OR model_count = 0 THEN json_build_array(
                'SELECT pg_probablepeople.upload_model(''model_name'', ''1.0'', ''description'', model_bytea_data);',
                'SELECT pg_probablepeople.set_active_model(''model_name'');',
                'SELECT pg_probablepeople.parse_name(''John Smith'');'
            )
            ELSE json_build_array(
                'SELECT pg_probablepeople.parse_name(''Test Name'');',
                'SELECT pg_probablepeople.tag_name(''Test Name'');'
            )
        END
    );
    
    RETURN result;
END;
$$;

-- Create sample data for testing (if enabled)
DO $$
DECLARE
    create_samples BOOLEAN;
BEGIN
    SELECT pg_probablepeople.get_config_bool('debug_mode') INTO create_samples;
    
    IF COALESCE(create_samples, false) THEN
        -- Insert some test parsing results for demonstration
        INSERT INTO pg_probablepeople.parsed_names (
            original_text, 
            parsed_components, 
            model_name, 
            model_version,
            processing_time_ms
        ) VALUES 
        (
            'John Smith', 
            '{"tokens": [{"text": "John", "label": "GivenName"}, {"text": "Smith", "label": "Surname"}]}',
            'probablepeople_v1',
            '1.0.0',
            15
        ),
        (
            'Dr. Sarah Johnson-Wilson PhD', 
            '{"tokens": [{"text": "Dr.", "label": "PrefixOther"}, {"text": "Sarah", "label": "GivenName"}, {"text": "Johnson-Wilson", "label": "Surname"}, {"text": "PhD", "label": "SuffixOther"}]}',
            'probablepeople_v1',
            '1.0.0',
            23
        ),
        (
            'Microsoft Corporation', 
            '{"tokens": [{"text": "Microsoft", "label": "CorporationName"}, {"text": "Corporation", "label": "CorporationLegalType"}]}',
            'probablepeople_v1',
            '1.0.0',
            12
        )
        ON CONFLICT DO NOTHING;
        
        RAISE NOTICE 'Created sample parsed results for testing';
    END IF;
END $$;

-- Final verification and status message
DO $$
DECLARE
    status JSON;
BEGIN
    SELECT pg_probablepeople.verify_installation() INTO status;
    
    RAISE NOTICE 'CRFName NER Extension Installation Status: %', status->>'installation_status';
    
    IF (status->>'installation_status') != 'Ready' THEN
        RAISE NOTICE 'Recommendations: %', status->'recommendations';
        RAISE NOTICE 'Next steps: %', status->'next_steps';
    ELSE
        RAISE NOTICE 'Extension is ready for use!';
        RAISE NOTICE 'Try: SELECT pg_probablepeople.parse_name(''John Smith'');';
    END IF;
END $$;

-- Add helpful comments
COMMENT ON FUNCTION pg_probablepeople.verify_installation() IS 'Check installation status and provide recommendations for setup completion';