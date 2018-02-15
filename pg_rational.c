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

#ifdef HAVE_LONG_INT_64
  #define add_int64_overflow __builtin_saddl_overflow
  #define mul_int64_overflow __builtin_smull_overflow
#elif
  #define add_int64_overflow __builtin_saddll_overflow
  #define mul_int64_overflow __builtin_smulll_overflow
#endif

#define SMALLEST_INT64 (-9223372036854775807 - 1)

PG_MODULE_MAGIC;

typedef struct {
  int64 numer;
  int64 denom;
} Rational;

static int64 positive_gcd(int64, int64);

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

  // prevent negative denominator, but do not negate the
  // smallest value -- that would produce overflow
  if(d >= 0 || n == SMALLEST_INT64 || d == SMALLEST_INT64) {
    result->numer = n;
    result->denom = d;
  } else {
    result->numer = -n;
    result->denom = -d;
  }

  PG_RETURN_POINTER(result);
}

Datum
rational_out(PG_FUNCTION_ARGS) {
  Rational *r = (Rational *)PG_GETARG_POINTER(0);
  int64 common = positive_gcd(r->numer, r->denom);

  PG_RETURN_CSTRING(
    psprintf(INT64_FORMAT "/" INT64_FORMAT,
      r->numer / common, r->denom / common)
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

/************* ARITHMETIC **************/

PG_FUNCTION_INFO_V1(rational_add);

Datum
rational_add(PG_FUNCTION_ARGS) {
  Rational *x = (Rational *)PG_GETARG_POINTER(0),
           *y = (Rational *)PG_GETARG_POINTER(1);
  int64 xnyd, ynxd, numer, denom;
  bool nxyd_bad, ynxd_bad, numer_bad, denom_bad;
  Rational *result = palloc(sizeof(Rational));

  nxyd_bad  = mul_int64_overflow(x->numer, y->denom, &xnyd);
  ynxd_bad  = mul_int64_overflow(y->numer, x->denom, &ynxd);
  numer_bad = add_int64_overflow(xnyd,     ynxd,     &numer);
  denom_bad = mul_int64_overflow(x->denom, y->denom, &denom);

  if(nxyd_bad || ynxd_bad || numer_bad || denom_bad) {
    ereport(ERROR,
      (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
       errmsg("intermediate value overflow in rational addition"))
    );
  }
  result->numer = numer;
  result->denom = denom;
  PG_RETURN_POINTER(result);
}

/************** INTERNAL ***************/

int64 positive_gcd(int64 a, int64 b) {
  int64 temp;
  while (b != 0) {
    temp = a % b;
    a = b;
    b = temp;
  }
  return a >= 0 ? a : -a;
}
