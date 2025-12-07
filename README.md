# pg_probablepeople

A Postgres extension based on the [probablepeople](https://github.com/datamade/probablepeople) Python package for parsing names into their components.

This extension wraps the CRF suite C/C++ library to provide name parsing capabilities natively within PostgreSQL.

This package was coded with Antigravity and Gemini 3.

## Features

- **`parse_name(text)`**: Returns a table of tokens and their predicted labels.
- **`tag_name(text)`**: Returns a JSONB object with the tagged components.

## Installation

### Using Docker (Recommended)

The easiest way to build, install, and test the extension is using Docker, which avoids local system dependency and permission issues.

1.  **Build the Docker image:**
    ```bash
    docker-compose build
    ```

2.  **Start the database container:**
    ```bash
    docker-compose up -d
    ```

3.  **Connect to the database:**
    ```bash
    docker-compose exec db psql -U postgres
    ```

4.  **Enable the extension:**
    ```sql
    CREATE EXTENSION pg_probablepeople;
    ```

### Manual Installation (Advanced)

If you are on Linux or have a properly configured PostgreSQL development environment on macOS (avoiding SIP issues):

1.  Ensure you have PostgreSQL development headers installed (`postgresql-server-dev-X.X`).
2.  Run:
    ```bash
    make
    sudo make install
    ```
    *Note: On recent macOS versions, `sudo make install` may fail due to System Integrity Protection (SIP) depending on your Postgres installation location.*

## Usage

### Parsing a Name to Table
The `parse_name` function returns a row for each token in the input string.

```sql
SELECT * FROM parse_name('Mr. John Doe');
```

**Output:**
```text
 token |     label     
-------+---------------
 Mr.   | PrefixMarital
 John  | GivenName
 Doe   | Surname
(3 rows)
```

### Tagging a Name to JSON
The `tag_name` function returns a JSONB object where keys are the predicted labels.

```sql
SELECT tag_name('Mr. John Doe');
```

**Output:**
```json
{"GivenName": "John", "Surname": "Doe", "PrefixMarital": "Mr."}
```

## Testing

To run the regression test suite within the Docker container:

```bash
docker-compose exec -u root db chown -R postgres:postgres .
docker-compose exec -u postgres db make installcheck
```
