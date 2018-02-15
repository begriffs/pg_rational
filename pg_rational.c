/* Based on
 * https://www.postgresql.org/docs/10/static/xtypes.html
 * http://big-elephants.com/2015-10/writing-postgres-extensions-part-i/
 * http://big-elephants.com/2015-10/writing-postgres-extensions-part-ii/
 * https://www.postgresql.org/docs/10/static/source.html
 * https://www.postgresql.org/docs/10/static/extend-extensions.html
 * */

#include "postgres.h"
#include "fmgr.h"
#include "libpq/pqformat.h" /* needed for send/recv functions */


PG_MODULE_MAGIC;

typedef struct {
  int64 numer;
  int64 denom;
} Rational;

/***************** IO ******************/

PG_FUNCTION_INFO_V1(rational_in);
PG_FUNCTION_INFO_V1(rational_out);
PG_FUNCTION_INFO_V1(rational_recv);
PG_FUNCTION_INFO_V1(rational_send);

Datum
rational_in(PG_FUNCTION_ARGS) {
  char *s = PG_GETARG_CSTRING(0);
  int64 n, d;
  Rational *result = palloc(sizeof(Rational));

  errno = 0;
  if(sscanf(s, INT64_FORMAT "/" INT64_FORMAT, &n, &d) != 2) {
    ereport(ERROR,
      (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
       errmsg("invalid input syntax for fraction: \"%s\"", s))
    );
  }
  if(errno == ERANGE) {
    ereport(ERROR,
      (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
       errmsg("numerator or denominator outside valid int64 value: \"%s\"", s))
    );
  }
  if(d == 0) {
    ereport(ERROR,
      (errcode(ERRCODE_DIVISION_BY_ZERO),
       errmsg("fraction cannot have zero denominator: \"%s\"", s))
    );
  }

  // prevent negative denominator
  result->numer = d >= 0 ? n : -n;
  result->denom = d >= 0 ? d : -d;

  PG_RETURN_POINTER(result);
}

Datum
rational_out(PG_FUNCTION_ARGS) {
  Rational *r = (Rational *)PG_GETARG_POINTER(0);

  PG_RETURN_CSTRING(
    psprintf(INT64_FORMAT "/" INT64_FORMAT, r->numer, r->denom)
  );
}

Datum
rational_recv(PG_FUNCTION_ARGS) {
  StringInfo buf   = (StringInfo)PG_GETARG_POINTER(0);
  Rational *result = palloc(sizeof(Rational));

  result->numer = pq_getmsgint64(buf);
  result->denom = pq_getmsgint64(buf);
  PG_RETURN_POINTER(result);
}

Datum
rational_send(PG_FUNCTION_ARGS) {
  Rational *r = (Rational *)PG_GETARG_POINTER(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, r->numer);
  pq_sendint64(&buf, r->denom);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}
