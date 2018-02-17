/* Based on
 * https://www.postgresql.org/docs/10/static/xtypes.html
 * http://big-elephants.com/2015-10/writing-postgres-extensions-part-i/
 * http://big-elephants.com/2015-10/writing-postgres-extensions-part-ii/
 * https://www.postgresql.org/docs/10/static/source.html
 * https://www.postgresql.org/docs/10/static/extend-extensions.html
 * */

#include "postgres.h"
#include "fmgr.h"
#include "access/hash.h"
#include "libpq/pqformat.h" /* needed for send/recv functions */
#include <limits.h>

#ifdef HAVE_LONG_INT_64
  #define add_int64_overflow __builtin_saddl_overflow
  #define mul_int64_overflow __builtin_smull_overflow
#elif
  #define add_int64_overflow __builtin_saddll_overflow
  #define mul_int64_overflow __builtin_smulll_overflow
#endif

PG_MODULE_MAGIC;

typedef struct {
  int64 numer;
  int64 denom;
} Rational;

static int64 gcd(int64, int64);
static bool  simplify(Rational*);
static int32 cmp(Rational*, Rational*);

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
  if(d >= 0 || n == INT64_MIN || d == INT64_MIN) {
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
  if(result->denom == 0) {
    ereport(ERROR,
      (errcode(ERRCODE_DIVISION_BY_ZERO),
       errmsg("fraction cannot have zero denominator: \""
        INT64_FORMAT "/" INT64_FORMAT "\"",
        result->numer, result->denom))
    );
  }
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

PG_FUNCTION_INFO_V1(rational_simplify);
PG_FUNCTION_INFO_V1(rational_add);
PG_FUNCTION_INFO_V1(rational_mul);
PG_FUNCTION_INFO_V1(rational_neg);

Datum
rational_simplify(PG_FUNCTION_ARGS) {
  Rational *in  = (Rational *)PG_GETARG_POINTER(0);
  Rational *out = palloc(sizeof(Rational));

  memcpy(out, in, sizeof(Rational));
  simplify(out);

  PG_RETURN_POINTER(out);
}

Datum
rational_add(PG_FUNCTION_ARGS) {
  Rational x, y;
  int64 xnyd, ynxd, numer, denom;
  bool nxyd_bad, ynxd_bad, numer_bad, denom_bad;
  Rational *result;

  // we may modify these, so make a copy of args
  memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
  memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));

retry_add:
  nxyd_bad  = mul_int64_overflow(x.numer, y.denom, &xnyd);
  ynxd_bad  = mul_int64_overflow(y.numer, x.denom, &ynxd);
  numer_bad = add_int64_overflow(xnyd,    ynxd,    &numer);
  denom_bad = mul_int64_overflow(x.denom, y.denom, &denom);

  if(nxyd_bad || ynxd_bad || numer_bad || denom_bad) {
    // overflow in intermediate value
    if(!simplify(&x) && !simplify(&y)) {
      // neither fraction could reduce, cannot proceed
      ereport(ERROR,
        (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
         errmsg("intermediate value overflow in rational addition"))
      );
    }
    // the fraction(s) reduced, good for one more retry
    goto retry_add;
  }
  result = palloc(sizeof(Rational));
  result->numer = numer;
  result->denom = denom;
  PG_RETURN_POINTER(result);
}

Datum
rational_mul(PG_FUNCTION_ARGS) {
  Rational x, y;
  int64 numer, denom;
  bool numer_bad, denom_bad;
  Rational *result;

  // we may modify these, so make a copy of args
  memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
  memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));

retry_mul:
  numer_bad = mul_int64_overflow(x.numer, y.numer, &numer);
  denom_bad = mul_int64_overflow(x.denom, y.denom, &denom);

  if(numer_bad || denom_bad) {
    // overflow in intermediate value
    if(!simplify(&x) && !simplify(&y)) {
      // neither fraction could reduce, cannot proceed
      ereport(ERROR,
        (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
         errmsg("intermediate value overflow in rational multiplication"))
      );
    }
    // the fraction(s) reduced, good for one more retry
    goto retry_mul;
  }

  result = palloc(sizeof(Rational));
  result->numer = numer;
  result->denom = denom;
  PG_RETURN_POINTER(result);
}

Datum
rational_neg(PG_FUNCTION_ARGS) {
  Rational *out = palloc(sizeof(Rational));
  memcpy(out, PG_GETARG_POINTER(0), sizeof(Rational));

  if(out->numer == INT64_MIN) {
    simplify(out);
  }

  if(out->numer == INT64_MIN) {
    // denom can't be MIN too or fraction would be 1/1
    out->denom *= -1;
  } else {
    // the normal case
    out->numer *= -1;
  }

  PG_RETURN_POINTER(out);
}

/*************** UTILITY ***************/

PG_FUNCTION_INFO_V1(rational_hash);

Datum
rational_hash(PG_FUNCTION_ARGS) {
  Rational x;
  memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
  // hash_any works at binary level, so we must simplify fraction
  simplify(&x);
  return hash_any((const unsigned char *)&x, sizeof(Rational));
};

/************* COMPARISON **************/

PG_FUNCTION_INFO_V1(rational_cmp);
PG_FUNCTION_INFO_V1(rational_eq);
PG_FUNCTION_INFO_V1(rational_ne);
PG_FUNCTION_INFO_V1(rational_lt);
PG_FUNCTION_INFO_V1(rational_le);
PG_FUNCTION_INFO_V1(rational_gt);
PG_FUNCTION_INFO_V1(rational_ge);

Datum
rational_cmp(PG_FUNCTION_ARGS) {
  PG_RETURN_INT32(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)));
}

Datum
rational_eq(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) == 0
  );
}

Datum
rational_ne(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) != 0
  );
}

Datum
rational_lt(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) < 0
  );
}

Datum
rational_le(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) <= 0
  );
}

Datum
rational_gt(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) > 0
  );
}

Datum
rational_ge(PG_FUNCTION_ARGS) {
  PG_RETURN_BOOL(
    cmp((Rational*)PG_GETARG_POINTER(0), (Rational*)PG_GETARG_POINTER(1)) >= 0
  );
}

/************** INTERNAL ***************/

int64 gcd(int64 a, int64 b) {
  int64 temp;
  while (b != 0) {
    temp = a % b;
    a = b;
    b = temp;
  }
  return a;
}

bool simplify(Rational *r) {
  int64 common = gcd(r->numer, r->denom);

  r->numer /= common;
  r->denom /= common;

  // prevent negative denominator, but do not negate the
  // smallest value -- that would produce overflow
  if(r->denom < 0 && r->numer != INT64_MIN && r->denom != INT64_MIN) {
    r->numer *= -1;
    r->denom *= -1;
  }

  return (common != 1) && (common != -1);
}

int32 cmp(Rational* a, Rational* b) {
  Rational x, y;
  int64 cross1, cross2;
  bool cross1_bad, cross2_bad;

  // we may modify these, so make a copy of args
  memcpy(&x, a, sizeof(Rational));
  memcpy(&y, b, sizeof(Rational));

retry_cmp:
  cross1_bad = mul_int64_overflow(x.numer, y.denom, &cross1);
  cross2_bad = mul_int64_overflow(x.denom, y.numer, &cross2);

  if(cross1_bad || cross2_bad) {
    // overflow in intermediate value
    if(!simplify(&x) && !simplify(&y)) {
      // neither fraction could reduce, cannot proceed
      ereport(ERROR,
        (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
         errmsg("intermediate value overflow in rational comparison"))
      );
    }
    // the fraction(s) reduced, good for one more retry
    goto retry_cmp;
  }
  return (cross1 > cross2) - (cross1 < cross2);
}
