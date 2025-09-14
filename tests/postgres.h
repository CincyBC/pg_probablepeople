#ifndef POSTGRES_MOCK_H
#define POSTGRES_MOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// Mock memory management
#define palloc(size) malloc(size)
#define palloc0(size) calloc(1, size)
#define repalloc(ptr, size) realloc(ptr, size)
#define pfree(ptr) free(ptr)

// Mock error reporting
#define ereport(level, ...) mock_ereport(__VA_ARGS__)
#define errmsg(...) mock_errmsg(__VA_ARGS__)
#define DEBUG1 1
#define LOG 2
#define WARNING 3
#define ERROR 4
#define ERRCODE_IO_ERROR 1
#define ERRCODE_CONNECTION_FAILURE 2
#define ERRCODE_SQL_STATEMENT_NOT_YET_COMPLETE 3

void mock_ereport(const char *fmt, ...);
const char* mock_errmsg(const char *fmt, ...);

// Mock memory context
typedef void *MemoryContext;
#define MemoryContextSwitchTo(context) do {} while (0)
#define crf_memory_context ((void*)0)

// Mock PostgreSQL types
typedef char bytea;
#define VARDATA(p) (p)
#define VARSIZE(p) (strlen(p))
#define VARHDRSZ 0

typedef void *Datum;
#define DatumGetByteaP(d) ((bytea *)(d))

// Mock JSONB
typedef struct JsonbValue { int type; } JsonbValue;
typedef void JsonbParseState;
#define jbvObject 1
#define jbvString 2
#define jbvNumeric 3
#define WJB_BEGIN_OBJECT 1
#define WJB_END_OBJECT 2
#define WJB_BEGIN_ARRAY 3
#define WJB_END_ARRAY 4
#define WJB_KEY 5
#define WJB_VALUE 6
JsonbValue *pushJsonbValue(JsonbParseState **state, int action, JsonbValue *val);
char *JsonbToCString(void* a, void* b, int c);


// Mock SPI
#define SPI_OK_CONNECT 1
#define SPI_OK_SELECT 2
#define SPI_OK_INSERT 3
#define SPI_OK_UPDATE 4
#define SPI_processed 0
typedef void SPITupleTable;
typedef void TupleDesc;
int SPI_connect(void);
int SPI_execute(const char *query, bool read_only, int count);
void SPI_finish(void);
char *SPI_getvalue(void *tup, void *tupdesc, int col);
Datum SPI_getbinval(void *tup, void *tupdesc, int col, bool *isnull);


// Mock other PostgreSQL functions
#define pstrdup(s) strdup(s)
#define DirectFunctionCall1(f, arg) (0)
#define Float4GetDatum(f) (f)

// Mock header files
#define POSTGRES_H
#define UTILS_BUILTINS_H
#define UTILS_MEMUTILS_H
#define UTILS_BUILTINS_H
#define UTILS_JSON_H
#define UTILS_JSONB_H
#define EXECUTOR_SPI_H
#define CATALOG_PG_TYPE_H
#define FMGR_H

#endif // POSTGRES_MOCK_H
