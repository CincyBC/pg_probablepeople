-- sql/tables/001_crf_models.sql
-- Table for storing CRF model binary data

CREATE TABLE IF NOT EXISTS pg_probablepeople.crf_models (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL,
    version VARCHAR(20) NOT NULL DEFAULT '1.0',
    description TEXT,
    model_data BYTEA NOT NULL,
    feature_template JSON,
    metadata JSON DEFAULT '{}',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT FALSE,
    checksum VARCHAR(64),
    model_size_bytes BIGINT,
    
    -- Ensure only one active model at a time
    CONSTRAINT unique_active_model EXCLUDE (is_active WITH =) WHERE (is_active = TRUE),
    
    -- Ensure model data is not null
    CONSTRAINT model_data_not_null CHECK (model_data IS NOT NULL),
    
    -- Version format check
    CONSTRAINT version_format CHECK (version ~ '^[0-9]+\.[0-9]+(\.[0-9]+)?$')
);

-- Set storage to EXTERNAL for better performance with large binary data
ALTER TABLE pg_probablepeople.crf_models ALTER COLUMN model_data SET STORAGE EXTERNAL;

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_crf_models_name ON pg_probablepeople.crf_models(name);
CREATE INDEX IF NOT EXISTS idx_crf_models_active ON pg_probablepeople.crf_models(is_active) WHERE is_active = TRUE;
CREATE INDEX IF NOT EXISTS idx_crf_models_version ON pg_probablepeople.crf_models(name, version);
CREATE INDEX IF NOT EXISTS idx_crf_models_created ON pg_probablepeople.crf_models(created_at DESC);

-- Add trigger to update updated_at timestamp
CREATE OR REPLACE FUNCTION pg_probablepeople.update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_crf_models_updated_at
    BEFORE UPDATE ON pg_probablepeople.crf_models
    FOR EACH ROW
    EXECUTE FUNCTION pg_probablepeople.update_updated_at_column();

-- Add trigger to calculate model size
CREATE OR REPLACE FUNCTION pg_probablepeople.calculate_model_size()
RETURNS TRIGGER AS $$
BEGIN
    NEW.model_size_bytes = LENGTH(NEW.model_data);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER calculate_crf_model_size
    BEFORE INSERT OR UPDATE ON pg_probablepeople.crf_models
    FOR EACH ROW
    EXECUTE FUNCTION pg_probablepeople.calculate_model_size();

-- Add comments for documentation
COMMENT ON TABLE pg_probablepeople.crf_models IS 'Storage for CRF model binary data and metadata';
COMMENT ON COLUMN pg_probablepeople.crf_models.model_data IS 'Binary CRF model data stored as BYTEA';
COMMENT ON COLUMN pg_probablepeople.crf_models.feature_template IS 'JSON template for feature extraction configuration';
COMMENT ON COLUMN pg_probablepeople.crf_models.metadata IS 'Additional model metadata including training parameters';
COMMENT ON COLUMN pg_probablepeople.crf_models.checksum IS 'SHA-256 checksum of model_data for integrity verification';
COMMENT ON COLUMN pg_probablepeople.crf_models.is_active IS 'Only one model can be active at a time';