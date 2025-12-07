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

## Training Models

The extension includes a C-based training tool that allows you to retrain the CRF models with custom data. This is useful when you encounter names that are mislabeled or when you want to add support for new naming patterns.

### Training Data Format

Training data is stored in XML files in the `name_data/` directory:
- **`person_labeled.xml`**: Person name examples (GivenName, Surname, MiddleName, Prefix, Suffix, etc.)
- **`company_labeled.xml`**: Company name examples (CorporationName, CorporationLegalType, etc.)

**XML Format Example:**
```xml
<NameCollection>
  <Name><PrefixOther>President</PrefixOther> <GivenName>Joe</GivenName> <Surname>Biden</Surname></Name>
  <Name><CorporationName>Google</CorporationName> <CorporationLegalType>Inc.</CorporationLegalType></Name>
</NameCollection>
```

### Common Label Types

**Person Names:**
- `GivenName`, `Surname`, `MiddleName`, `MiddleInitial`
- `PrefixMarital` (Mr., Mrs., Ms.), `PrefixOther` (Dr., President)
- `SuffixGenerational` (Jr., Sr., III), `SuffixOther` (Esq., PhD)

**Company Names:**
- `CorporationName`, `CorporationLegalType` (Inc., LLC, Corp.)
- `CorporationNameOrganization` (Corporation, Company, Group)

### Adding New Training Examples

When you encounter a mislabeled name:

1. **Add examples to the training data:**
   ```bash
   # Edit the appropriate XML file
   vim name_data/person_labeled.xml
   # or
   vim name_data/company_labeled.xml
   ```

2. **Add 3-5 similar examples** to give the model enough signal to learn the pattern.

### Training the Model

**Inside Docker (Recommended):**

```bash
# 1. Copy updated training data to container
docker cp name_data/person_labeled.xml pg_probablepeople-db-1:/usr/src/pg_probablepeople/name_data/
docker cp name_data/company_labeled.xml pg_probablepeople-db-1:/usr/src/pg_probablepeople/name_data/

# 2. Build training tool and train the generic model
docker exec pg_probablepeople-db-1 bash -c "cd /usr/src/pg_probablepeople && \
  make -f Makefile.training training-tool && \
  ./train_model -t generic -p name_data/person_labeled.xml -c name_data/company_labeled.xml -o include/generic_learned_settings.crfsuite && \
  make install && \
  psql -U postgres -c 'DROP EXTENSION IF EXISTS pg_probablepeople CASCADE; CREATE EXTENSION pg_probablepeople;'"

# 3. Copy trained model back to host (for version control)
docker cp pg_probablepeople-db-1:/usr/src/pg_probablepeople/include/generic_learned_settings.crfsuite ./include/generic_learned_settings.crfsuite
```

**Training Options:**
- `-t generic`: Train a unified model for both person and company names (recommended)
- `-t person`: Train person-only model
- `-t company`: Train company-only model
- `-p <file>`: Person training data XML
- `-c <file>`: Company training data XML
- `-o <file>`: Output model file

**Example: Train person-only model:**
```bash
./train_model name_data/person_labeled.xml -o include/person_learned_settings.crfsuite
```

### Workflow Summary

1. Encounter mislabeled name → Add examples to XML
2. Copy updated XML to container
3. Rebuild and retrain model
4. Test the new model
5. Copy model back to host and commit changes

## Testing

To run the regression test suite within the Docker container:

```bash
docker-compose exec -u root db chown -R postgres:postgres .
docker-compose exec -u postgres db make installcheck
```
