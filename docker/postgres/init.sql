CREATE TABLE IF NOT EXISTS exchanges (
    exchange_code VARCHAR(10) PRIMARY KEY,
    exchange_name VARCHAR(255) NOT NULL,
    city VARCHAR(50) NOT NULL,
    country VARCHAR(50) NOT NULL
);

INSERT INTO exchanges (exchange_code, exchange_name, city, country)
VALUES
    ('NASDAQ', 'National Association of Securities Dealers Automated Quotations', 'New York City',
     'United States of America'),
    ('NYSE', 'New York Stock Exchange', 'New York City', 'United States of America')
ON CONFLICT (exchange_code) DO UPDATE SET
    exchange_name = EXCLUDED.exchange_name,
    city = EXCLUDED.city,
    country = EXCLUDED.country;

CREATE TABLE IF NOT EXISTS exchange_values (
    exchange_value_id SERIAL PRIMARY KEY,
    exchange_code VARCHAR(10) REFERENCES exchanges (exchange_code) NOT NULL,
    value DECIMAL(12, 2) NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL,
    CONSTRAINT unique_exchange_ts UNIQUE (exchange_code, timestamp)
);

CREATE TABLE IF NOT EXISTS securities (
    security_id SERIAL PRIMARY KEY,
    symbol VARCHAR(10) NOT NULL,
    company_name VARCHAR(255) NOT NULL,
    exchange_code VARCHAR(10) REFERENCES exchanges (exchange_code) NOT NULL,
    CONSTRAINT unique_symbol_exchange UNIQUE (symbol, exchange_code)
);

CREATE TABLE IF NOT EXISTS security_prices (
    security_price_id BIGSERIAL PRIMARY KEY,
    security_id INTEGER REFERENCES securities (security_id) NOT NULL,
    price DECIMAL(12, 2) NOT NULL,
    currency CHAR(3) NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL,
    CONSTRAINT unique_security_ts UNIQUE (security_id, timestamp)
);
