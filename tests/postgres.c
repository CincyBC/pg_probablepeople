#include "postgres.h"

void mock_ereport(const char *fmt, ...) {
    // no-op
}

const char* mock_errmsg(const char *fmt, ...) {
    return "mock error message";
}

JsonbValue *pushJsonbValue(JsonbParseState **state, int action, JsonbValue *val) {
    return NULL;
}

char *JsonbToCString(void* a, void* b, int c) {
    return "{}";
}

int SPI_connect(void) {
    return SPI_OK_CONNECT;
}

int SPI_execute(const char *query, bool read_only, int count) {
    return SPI_OK_SELECT;
}

void SPI_finish(void) {
    // no-op
}

char *SPI_getvalue(void *tup, void *tupdesc, int col) {
    return "mock value";
}

Datum SPI_getbinval(void *tup, void *tupdesc, int col, bool *isnull) {
    *isnull = false;
    return "mock data";
}
