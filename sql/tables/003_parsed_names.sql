-- sql/tables/003_parsed_names.sql
-- Table for caching parsed results and performance tracking

CREATE TABLE IF NOT EXISTS pg_probablepeople.parsed_names (
    id SERIAL PRIMARY KEY,
    original_text TEXT NOT NULL,
    parsed_components JSON NOT NULL,
    confidence_scores JSON,
    model_name VARCHAR(100),
    model_version VARCHAR(20),
    processing_time_ms INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    access_count INTEGER DEFAULT 1,
    
    -- Ensure reasonable processing times
    CONSTRAINT reasonable_processing_time CHECK (processing_time_ms >= 0 AND processing_time_ms < 300000), -- 5 minutes max
    
    -- Ensure confidence scores are valid JSON
    CONSTRAINT valid_confidence_scores CHECK (confidence_scores IS NULL OR confidence_scores::TEXT ~ '^[\[\{].*[\]\}]$')
);

-- Create unique index on original_text for caching (with hash for performance on long texts)
CREATE UNIQUE INDEX IF NOT EXISTS idx_parsed_names_text_hash ON pg_probablepeople.parsed_names 
    USING HASH (MD5(original_text));

-- Create regular index on original_text for exact lookups
CREATE INDEX IF NOT EXISTS idx_parsed_names_text ON pg_probablepeople.parsed_names(original_text);

-- Create indexes for performance monitoring
CREATE INDEX IF NOT EXISTS idx_parsed_names_model ON pg_probablepeople.parsed_names(model_name, model_version);
CREATE INDEX IF NOT EXISTS idx_parsed_names_created ON pg_probablepeople.parsed_names(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_parsed_names_performance ON pg_probablepeople.parsed_names(processing_time_ms);
CREATE INDEX IF NOT EXISTS idx_parsed_names_accessed ON pg_probablepeople.parsed_names(last_accessed DESC);

-- Create GIN index on parsed_components for JSON queries
CREATE INDEX IF NOT EXISTS idx_parsed_names_components_gin ON pg_probablepeople.parsed_names 
    USING GIN (parsed_components);

-- Add trigger to update last_accessed on SELECT (via function)
CREATE OR REPLACE FUNCTION pg_probablepeople.update_access_stats()
RETURNS TRIGGER AS $$
BEGIN
    NEW.last_accessed = CURRENT_TIMESTAMP;
    NEW.access_count = COALESCE(OLD.access_count, 0) + 1;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Function to clean up old cache entries
CREATE OR REPLACE FUNCTION pg_probablepeople.cleanup_old_cache(older_than_hours INTEGER DEFAULT 168) -- 1 week default
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    WITH deleted AS (
        DELETE FROM pg_probablepeople.parsed_names 
        WHERE created_at < NOW() - INTERVAL '1 hour' * older_than_hours
        AND access_count <= 1  -- Only delete entries that were accessed once or less
        RETURNING id
    )
    SELECT COUNT(*) INTO deleted_count FROM deleted;
    
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Function to get cache statistics
CREATE OR REPLACE FUNCTION pg_probablepeople.get_cache_stats()
RETURNS TABLE(
    total_entries BIGINT,
    total_size_mb FLOAT,
    avg_processing_time_ms FLOAT,
    cache_hit_rate FLOAT,
    oldest_entry TIMESTAMP,
    newest_entry TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        COUNT(*)::BIGINT as total_entries,
        (PG_TOTAL_RELATION_SIZE('pg_probablepeople.parsed_names')::FLOAT / 1024 / 1024) as total_size_mb,
        AVG(processing_time_ms)::FLOAT as avg_processing_time_ms,
        CASE 
            WHEN COUNT(*) > 0 THEN (SUM(access_count - 1)::FLOAT / COUNT(*)::FLOAT)
            ELSE 0.0 
        END as cache_hit_rate,
        MIN(created_at) as oldest_entry,
        MAX(created_at) as newest_entry
    FROM pg_probablepeople.parsed_names;
END;
$$ LANGUAGE plpgsql;

-- Function to find similar cached entries (useful for fuzzy matching)
CREATE OR REPLACE FUNCTION pg_probablepeople.find_similar_parsed_names(
    input_text TEXT, 
    similarity_threshold FLOAT DEFAULT 0.8,
    max_results INTEGER DEFAULT 10
)
RETURNS TABLE(
    original_text TEXT,
    parsed_components JSON,
    similarity_score FLOAT,
    created_at TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        pn.original_text,
        pn.parsed_components,
        SIMILARITY(input_text, pn.original_text) as similarity_score,
        pn.created_at
    FROM pg_probablepeople.parsed_names pn
    WHERE SIMILARITY(input_text, pn.original_text) >= similarity_threshold
    ORDER BY similarity_score DESC
    LIMIT max_results;
END;
$$ LANGUAGE plpgsql;

-- Create a view for recent parsing activity
CREATE OR REPLACE VIEW pg_probablepeople.recent_parsing_activity AS
SELECT 
    original_text,
    model_name,
    model_version,
    processing_time_ms,
    access_count,
    created_at,
    last_accessed,
    EXTRACT(EPOCH FROM (last_accessed - created_at)) / 3600 as hours_since_created
FROM pg_probablepeople.parsed_names
WHERE created_at >= NOW() - INTERVAL '24 hours'
ORDER BY created_at DESC;

-- Create materialized view for performance analytics (refresh manually/via cron)
CREATE MATERIALIZED VIEW IF NOT EXISTS pg_probablepeople.parsing_performance_summary AS
SELECT 
    DATE_TRUNC('hour', created_at) as hour_bucket,
    model_name,
    model_version,
    COUNT(*) as parse_count,
    AVG(processing_time_ms) as avg_processing_time,
    MIN(processing_time_ms) as min_processing_time,
    MAX(processing_time_ms) as max_processing_time,
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY processing_time_ms) as median_processing_time,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY processing_time_ms) as p95_processing_time
FROM pg_probablepeople.parsed_names
WHERE created_at >= NOW() - INTERVAL '30 days'
GROUP BY DATE_TRUNC('hour', created_at), model_name, model_version
ORDER BY hour_bucket DESC;

-- Create index on materialized view
CREATE INDEX IF NOT EXISTS idx_parsing_performance_summary_hour ON pg_probablepeople.parsing_performance_summary(hour_bucket DESC);
CREATE INDEX IF NOT EXISTS idx_parsing_performance_summary_model ON pg_probablepeople.parsing_performance_summary(model_name, model_version);

-- Function to refresh performance summary
CREATE OR REPLACE FUNCTION pg_probablepeople.refresh_performance_summary()
RETURNS VOID AS $$
BEGIN
    REFRESH MATERIALIZED VIEW pg_probablepeople.parsing_performance_summary;
END;
$$ LANGUAGE plpgsql;

-- Add comments
COMMENT ON TABLE pg_probablepeople.parsed_names IS 'Cache table for parsed name results and performance tracking';
COMMENT ON COLUMN pg_probablepeople.parsed_names.original_text IS 'Original input text that was parsed';
COMMENT ON COLUMN pg_probablepeople.parsed_names.parsed_components IS 'JSON result from name parsing';
COMMENT ON COLUMN pg_probablepeople.parsed_names.confidence_scores IS 'Individual token confidence scores';
COMMENT ON COLUMN pg_probablepeople.parsed_names.processing_time_ms IS 'Time taken to parse in milliseconds';
COMMENT ON COLUMN pg_probablepeople.parsed_names.access_count IS 'Number of times this cached result was accessed';

COMMENT ON FUNCTION pg_probablepeople.cleanup_old_cache(INTEGER) IS 'Remove old cache entries older than specified hours';
COMMENT ON FUNCTION pg_probablepeople.get_cache_stats() IS 'Get cache performance statistics';
COMMENT ON FUNCTION pg_probablepeople.find_similar_parsed_names(TEXT, FLOAT, INTEGER) IS 'Find similar cached entries using text similarity';
COMMENT ON MATERIALIZED VIEW pg_probablepeople.parsing_performance_summary IS 'Hourly aggregated parsing performance metrics';