-- sql/tables/002_crf_config.sql
-- Configuration table for extension settings

CREATE TABLE IF NOT EXISTS pg_probablepeople.crf_config (
    parameter VARCHAR(100) PRIMARY KEY,
    value TEXT NOT NULL,
    description TEXT,
    data_type VARCHAR(20) DEFAULT 'text',
    is_readonly BOOLEAN DEFAULT FALSE,
    min_value NUMERIC,
    max_value NUMERIC,
    valid_values TEXT[],
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Validate data types
    CONSTRAINT config_data_type_check CHECK (
        data_type IN ('text', 'integer', 'float', 'boolean', 'json')
    )
);

-- Add trigger to update timestamp
CREATE TRIGGER update_crf_config_updated_at
    BEFORE UPDATE ON pg_probablepeople.crf_config
    FOR EACH ROW
    EXECUTE FUNCTION pg_probablepeople.update_updated_at_column();

-- Create function to safely get config values with type casting
CREATE OR REPLACE FUNCTION pg_probablepeople.get_config(param_name TEXT)
RETURNS TEXT AS $$
DECLARE
    config_value TEXT;
BEGIN
    SELECT value INTO config_value 
    FROM pg_probablepeople.crf_config 
    WHERE parameter = param_name;
    
    RETURN COALESCE(config_value, NULL);
END;
$$ LANGUAGE plpgsql STABLE;

-- Create function to get boolean config values
CREATE OR REPLACE FUNCTION pg_probablepeople.get_config_bool(param_name TEXT)
RETURNS BOOLEAN AS $$
DECLARE
    config_value TEXT;
BEGIN
    config_value := pg_probablepeople.get_config(param_name);
    RETURN CASE 
        WHEN LOWER(config_value) IN ('true', 't', 'yes', 'y', '1') THEN TRUE
        WHEN LOWER(config_value) IN ('false', 'f', 'no', 'n', '0') THEN FALSE
        ELSE NULL
    END;
END;
$$ LANGUAGE plpgsql STABLE;

-- Create function to get integer config values
CREATE OR REPLACE FUNCTION pg_probablepeople.get_config_int(param_name TEXT)
RETURNS INTEGER AS $$
DECLARE
    config_value TEXT;
BEGIN
    config_value := pg_probablepeople.get_config(param_name);
    RETURN CASE 
        WHEN config_value ~ '^[0-9]+$' THEN config_value::INTEGER
        ELSE NULL
    END;
END;
$$ LANGUAGE plpgsql STABLE;

-- Create function to get float config values
CREATE OR REPLACE FUNCTION pg_probablepeople.get_config_float(param_name TEXT)
RETURNS FLOAT AS $$
DECLARE
    config_value TEXT;
BEGIN
    config_value := pg_probablepeople.get_config(param_name);
    RETURN CASE 
        WHEN config_value ~ '^[0-9]*\.?[0-9]+$' THEN config_value::FLOAT
        ELSE NULL
    END;
END;
$$ LANGUAGE plpgsql STABLE;

-- Create function to set config values with validation
CREATE OR REPLACE FUNCTION pg_probablepeople.set_config(param_name TEXT, param_value TEXT)
RETURNS BOOLEAN AS $$
DECLARE
    config_row RECORD;
    is_valid BOOLEAN := TRUE;
BEGIN
    -- Get current config row
    SELECT * INTO config_row FROM pg_probablepeople.crf_config WHERE parameter = param_name;
    
    IF NOT FOUND THEN
        RAISE EXCEPTION 'Configuration parameter % does not exist', param_name;
    END IF;
    
    IF config_row.is_readonly THEN
        RAISE EXCEPTION 'Configuration parameter % is read-only', param_name;
    END IF;
    
    -- Validate based on data type
    CASE config_row.data_type
        WHEN 'integer' THEN
            IF param_value !~ '^[0-9]+$' THEN
                is_valid := FALSE;
            END IF;
            IF config_row.min_value IS NOT NULL AND param_value::INTEGER < config_row.min_value THEN
                is_valid := FALSE;
            END IF;
            IF config_row.max_value IS NOT NULL AND param_value::INTEGER > config_row.max_value THEN
                is_valid := FALSE;
            END IF;
            
        WHEN 'float' THEN
            IF param_value !~ '^[0-9]*\.?[0-9]+$' THEN
                is_valid := FALSE;
            END IF;
            IF config_row.min_value IS NOT NULL AND param_value::FLOAT < config_row.min_value THEN
                is_valid := FALSE;
            END IF;
            IF config_row.max_value IS NOT NULL AND param_value::FLOAT > config_row.max_value THEN
                is_valid := FALSE;
            END IF;
            
        WHEN 'boolean' THEN
            IF LOWER(param_value) NOT IN ('true', 'false', 't', 'f', 'yes', 'no', 'y', 'n', '1', '0') THEN
                is_valid := FALSE;
            END IF;
            
        WHEN 'json' THEN
            BEGIN
                PERFORM param_value::JSON;
            EXCEPTION WHEN OTHERS THEN
                is_valid := FALSE;
            END;
    END CASE;
    
    -- Check valid values if specified
    IF config_row.valid_values IS NOT NULL AND array_length(config_row.valid_values, 1) > 0 THEN
        IF param_value != ALL(config_row.valid_values) THEN
            is_valid := FALSE;
        END IF;
    END IF;
    
    IF NOT is_valid THEN
        RAISE EXCEPTION 'Invalid value % for parameter %', param_value, param_name;
    END IF;
    
    -- Update the value
    UPDATE pg_probablepeople.crf_config 
    SET value = param_value, updated_at = CURRENT_TIMESTAMP 
    WHERE parameter = param_name;
    
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql;

-- Add comments
COMMENT ON TABLE pg_probablepeople.crf_config IS 'Configuration parameters for CRF name parsing extension';
COMMENT ON FUNCTION pg_probablepeople.get_config(TEXT) IS 'Get configuration value as text';
COMMENT ON FUNCTION pg_probablepeople.get_config_bool(TEXT) IS 'Get configuration value as boolean';
COMMENT ON FUNCTION pg_probablepeople.get_config_int(TEXT) IS 'Get configuration value as integer';
COMMENT ON FUNCTION pg_probablepeople.get_config_float(TEXT) IS 'Get configuration value as float';
COMMENT ON FUNCTION pg_probablepeople.set_config(TEXT, TEXT) IS 'Set configuration value with validation';