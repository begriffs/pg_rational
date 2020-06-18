#include "postgres.h"
#include "fmgr.h"
#include "access/hash.h"
#include "libpq/pqformat.h"		/* send/recv functions */

#if PG_VERSION_NUM >= 110000
#include "common/int.h"			/* portable overflow detection */
#endif

#include <limits.h>
#include <math.h>

PG_MODULE_MAGIC;

typedef struct
{
	int32		numer;
	int32		denom;
}			Rational;

static int32 gcd(int32, int32);
static bool simplify(Rational *);
static int32 cmp(Rational *, Rational *);
static void neg(Rational *);
static Rational * add(Rational *, Rational *);
static Rational * mul(Rational *, Rational *);
static void mediant(Rational *, Rational *, Rational *);

/*
 ***************** IO ******************
 */

PG_FUNCTION_INFO_V1(rational_in);
PG_FUNCTION_INFO_V1(rational_in_float);
PG_FUNCTION_INFO_V1(rational_out);
PG_FUNCTION_INFO_V1(rational_out_float);
PG_FUNCTION_INFO_V1(rational_recv);
PG_FUNCTION_INFO_V1(rational_create);
PG_FUNCTION_INFO_V1(rational_embed);
PG_FUNCTION_INFO_V1(rational_send);

Datum
rational_in(PG_FUNCTION_ARGS)
{
	char	   *s = PG_GETARG_CSTRING(0),
			   *after;
	long long	n,
				d;
	Rational   *result = palloc(sizeof(Rational));

	if (!isdigit(*s) && *s != '-')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("Missing or invalid numerator")));

	n = strtoll(s, &after, 10);

	if (*after == '\0')
	{
		/* if just a number and no slash, interpret as an int */
		d = 1;
	}
	else
	{
		/* otherwise look for denominator */
		if (*after != '/')
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("Expecting '/' after number but found '%c'", *after)));
		if (*(++after) == '\0')
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("Expecting value after '/' but got '\\0'")));

		d = strtoll(after, &after, 10);
		if (*after != '\0')
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("Expecting '\\0' but found '%c'", *after)));

		if (d == 0)
			ereport(ERROR,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					 errmsg("fraction cannot have zero denominator")));
	}

	if (n < INT32_MIN || n > INT32_MAX || d < INT32_MIN || d > INT32_MAX)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("numerator or denominator outside valid int32 value")));

	/*
	 * prevent negative denominator, but do not negate the smallest value --
	 * that would produce overflow
	 */
	if (d >= 0 || n == INT32_MIN || d == INT32_MIN)
	{
		result->numer = (int32) n;
		result->denom = (int32) d;
	}
	else
	{
		result->numer = (int32) -n;
		result->denom = (int32) -d;
	}

	PG_RETURN_POINTER(result);
}

/*
  This function taken from John Kennedy's paper, "Algorithm To Convert a
  Decimal to a Fraction." Translated from Pascal.
*/
Datum
rational_in_float(PG_FUNCTION_ARGS)
{
	float8		target = PG_GETARG_FLOAT8(0),
				z,
				fnumer,
				fdenom,
				error;
	int32		prev_denom,
				sign;
	Rational   *result = palloc(sizeof(Rational));

	if (target == (int32) target)
	{
		result->numer = (int32) target;
		result->denom = 1;
		PG_RETURN_POINTER(result);
	}

	sign = target < 0.0 ? -1 : 1;
	target = fabs(target);

	if (!(target <= INT32_MAX)) /* also excludes NaNs */
	{
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("value too large for rational")));
	}
	z = target;
	prev_denom = 0;
	result->numer = (int32) round(target);
	result->denom = 1;
	do
	{
		z = 1.0 / (z - floor(z));
		fdenom = result->denom * floor(z) + prev_denom;
		fnumer = round(target * fdenom);
		if (fnumer > INT32_MAX || fdenom > INT32_MAX)
			break;
		prev_denom = result->denom;
		result->numer = (int32) fnumer;
		result->denom = (int32) fdenom;

		error = fabs(target - ((float8) result->numer / (float8) result->denom));
	} while (z != floor(z) && error >= 1e-12);

	result->numer *= sign;
	PG_RETURN_POINTER(result);
}

Datum
rational_out(PG_FUNCTION_ARGS)
{
	Rational   *r = (Rational *) PG_GETARG_POINTER(0);

	PG_RETURN_CSTRING(psprintf("%d/%d", r->numer, r->denom));
}

Datum
rational_out_float(PG_FUNCTION_ARGS)
{
	Rational   *r = (Rational *) PG_GETARG_POINTER(0);

	PG_RETURN_FLOAT8((float8) r->numer / (float8) r->denom);
}

Datum
rational_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	Rational   *result = palloc(sizeof(Rational));

	result->numer = pq_getmsgint(buf, sizeof(int32));
	result->denom = pq_getmsgint(buf, sizeof(int32));

	if (result->denom == 0)
		ereport(ERROR,
				(errcode(ERRCODE_DIVISION_BY_ZERO),
				 errmsg("fraction cannot have zero denominator: \"%d/%d\"",
						result->numer, result->denom)));

	PG_RETURN_POINTER(result);
}

Datum
rational_create(PG_FUNCTION_ARGS)
{
	int32		n = PG_GETARG_INT32(0),
				d = PG_GETARG_INT32(1);
	Rational   *result = palloc(sizeof(Rational));

	if (d == 0)
		ereport(ERROR,
				(errcode(ERRCODE_DIVISION_BY_ZERO),
				 errmsg("fraction cannot have zero denominator: \"%d/%d\"", n, d)));

	result->numer = n;
	result->denom = d;

	PG_RETURN_POINTER(result);
}

Datum
rational_embed(PG_FUNCTION_ARGS)
{
	int32		n = PG_GETARG_INT32(0);
	Rational   *result = palloc(sizeof(Rational));

	result->numer = n;
	result->denom = 1;

	PG_RETURN_POINTER(result);
}

Datum
rational_send(PG_FUNCTION_ARGS)
{
	Rational   *r = (Rational *) PG_GETARG_POINTER(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendint(&buf, r->numer, sizeof(int32));
	pq_sendint(&buf, r->denom, sizeof(int32));

	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 ************* ARITHMETIC **************
 */

PG_FUNCTION_INFO_V1(rational_simplify);
PG_FUNCTION_INFO_V1(rational_add);
PG_FUNCTION_INFO_V1(rational_sub);
PG_FUNCTION_INFO_V1(rational_mul);
PG_FUNCTION_INFO_V1(rational_div);
PG_FUNCTION_INFO_V1(rational_neg);

Datum
rational_simplify(PG_FUNCTION_ARGS)
{
	Rational   *in = (Rational *) PG_GETARG_POINTER(0);
	Rational   *out = palloc(sizeof(Rational));

	memcpy(out, in, sizeof(Rational));
	simplify(out);

	PG_RETURN_POINTER(out);
}

Datum
rational_add(PG_FUNCTION_ARGS)
{
	Rational	x,
				y;

	memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
	memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));

	PG_RETURN_POINTER(add(&x, &y));
}

Datum
rational_sub(PG_FUNCTION_ARGS)
{
	Rational	x,
				y;

	memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
	memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));

	neg(&y);
	PG_RETURN_POINTER(add(&x, &y));
}

Datum
rational_mul(PG_FUNCTION_ARGS)
{
	Rational	x,
				y;

	memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
	memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));

	PG_RETURN_POINTER(mul(&x, &y));
}

Datum
rational_div(PG_FUNCTION_ARGS)
{
	Rational	x,
				y;
	int32		tmp;

	memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));
	memcpy(&y, PG_GETARG_POINTER(1), sizeof(Rational));
	tmp = y.numer;
	y.numer = y.denom;
	y.denom = tmp;

	PG_RETURN_POINTER(mul(&x, &y));
}

Datum
rational_neg(PG_FUNCTION_ARGS)
{
	Rational   *out = palloc(sizeof(Rational));

	memcpy(out, PG_GETARG_POINTER(0), sizeof(Rational));
	neg(out);

	PG_RETURN_POINTER(out);
}

/*
 *************** UTILITY ***************
 */

PG_FUNCTION_INFO_V1(rational_hash);
PG_FUNCTION_INFO_V1(rational_intermediate);
PG_FUNCTION_INFO_V1(rational_intermediate_float);

Datum
rational_hash(PG_FUNCTION_ARGS)
{
	Rational	x;

	memcpy(&x, PG_GETARG_POINTER(0), sizeof(Rational));

	/*
	 * hash_any works at binary level, so we must simplify fraction
	 */
	simplify(&x);

	return hash_any((const unsigned char *) &x, sizeof(Rational));
}

Datum
rational_intermediate(PG_FUNCTION_ARGS)
{
	Rational	x,
				y,				/* arguments */
				lo = {0, 1},
				hi = {1, 0},	/* yes, an internal use of 1/0 */
			   *med = palloc(sizeof(Rational));

	/*
	 * x = coalesce(lo, arg[0]) y = coalesce(hi, arg[1])
	 */
	memcpy(&x,
		   PG_ARGISNULL(0) ? &lo : (Rational *) PG_GETARG_POINTER(0),
		   sizeof(Rational));
	memcpy(&y,
		   PG_ARGISNULL(1) ? &hi : (Rational *) PG_GETARG_POINTER(1),
		   sizeof(Rational));

	if (cmp(&x, &lo) < 0 || cmp(&y, &lo) < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("arguments must be non-negative")));

	if (cmp(&x, &y) >= 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("first argument must be strictly smaller than second")));

	while (true)
	{
		mediant(&lo, &hi, med);
		if (cmp(med, &x) < 1)
			memcpy(&lo, med, sizeof(Rational));
		else if (cmp(med, &y) > -1)
			memcpy(&hi, med, sizeof(Rational));
		else
			break;
	}

	PG_RETURN_POINTER(med);
}


/*
 ************* COMPARISON **************
 */


PG_FUNCTION_INFO_V1(rational_cmp);
PG_FUNCTION_INFO_V1(rational_eq);
PG_FUNCTION_INFO_V1(rational_ne);
PG_FUNCTION_INFO_V1(rational_lt);
PG_FUNCTION_INFO_V1(rational_le);
PG_FUNCTION_INFO_V1(rational_gt);
PG_FUNCTION_INFO_V1(rational_ge);

PG_FUNCTION_INFO_V1(rational_smaller);
PG_FUNCTION_INFO_V1(rational_larger);

Datum
rational_cmp(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(
					cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)));
}

Datum
rational_eq(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) == 0);
}

Datum
rational_ne(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) != 0);
}

Datum
rational_lt(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) < 0);
}

Datum
rational_le(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) <= 0);
}

Datum
rational_gt(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) > 0);
}

Datum
rational_ge(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
				   cmp((Rational *) PG_GETARG_POINTER(0), (Rational *) PG_GETARG_POINTER(1)) >= 0);
}

Datum
rational_smaller(PG_FUNCTION_ARGS)
{
	Rational   *a = (Rational *) PG_GETARG_POINTER(0),
			   *b = (Rational *) PG_GETARG_POINTER(1);

	PG_RETURN_POINTER(cmp(a, b) < 0 ? a : b);
}

Datum
rational_larger(PG_FUNCTION_ARGS)
{
	Rational   *a = (Rational *) PG_GETARG_POINTER(0),
			   *b = (Rational *) PG_GETARG_POINTER(1);

	PG_RETURN_POINTER(cmp(a, b) > 0 ? a : b);
}


/*
 ************** INTERNAL ***************
 */

#if PG_VERSION_NUM < 110000
/* Shims for the lack of src/include/common/int.h in old Postgres.
 * Less efficient because we don't leverage compiler builtins for
 * detecting overflow like the Postgres source does.
 */
static inline bool
pg_mul_s32_overflow(int32 a, int32 b, int32 *result)
{
	int64		res = (int64) a * (int64) b;

	if (res > PG_INT32_MAX || res < PG_INT32_MIN)
	{
		*result = 0x5EED;		/* to avoid spurious warnings */
		return true;
	}
	*result = (int32) res;
	return false;
}

static inline bool
pg_add_s32_overflow(int32 a, int32 b, int32 *result)
{
	int64		res = (int64) a + (int64) b;

	if (res > PG_INT32_MAX || res < PG_INT32_MIN)
	{
		*result = 0x5EED;		/* to avoid spurious warnings */
		return true;
	}
	*result = (int32) res;
	return false;
}
#endif							/* PG_VERSION_NUM */

int32
gcd(int32 a, int32 b)
{
	int32		temp;

	while (b != 0)
	{
		temp = a % b;
		a = b;
		b = temp;
	}

	return a;
}

bool
simplify(Rational * r)
{
	int32		common = gcd(r->numer, r->denom);

	/*
	 * tricky: avoid overflow from (INT32_MIN / -1)
	 */
	if (common != -1 || (r->numer != INT32_MIN && r->denom != INT32_MIN))
	{
		r->numer /= common;
		r->denom /= common;
	}

	/*
	 * prevent negative denominator, but do not negate the smallest value --
	 * that would produce overflow
	 */
	if (r->denom < 0 && r->numer != INT32_MIN && r->denom != INT32_MIN)
	{
		r->numer *= -1;
		r->denom *= -1;
	}
	return (common != 1) && (common != -1);
}

int32
cmp(Rational * a, Rational * b)
{
	/*
	 * Overflow is not an option, we need a total order so that btree indices
	 * do not die. Hence do the arithmetic in 64 bits.
	 */
	int64		cross1 = (int64) a->numer * (int64) b->denom,
				cross2 = (int64) a->denom * (int64) b->numer;

	return (cross1 > cross2) - (cross1 < cross2);
}

void
neg(Rational * r)
{
	if (r->numer == INT32_MIN)
	{
		simplify(r);

		/*
		 * check again
		 */
		if (r->numer == INT32_MIN)
		{
			/*
			 * denom can't be MIN too or fraction would have previously
			 * simplified to 1/1
			 */
			r->denom *= -1;
			return;
		}
	}
	r->numer *= -1;
}

Rational *
add(Rational * x, Rational * y)
{
	int32		xnyd,
				ynxd,
				numer,
				denom;
	bool		nxyd_bad,
				ynxd_bad,
				numer_bad,
				denom_bad;
	Rational   *result;

retry_add:
	nxyd_bad = pg_mul_s32_overflow(x->numer, y->denom, &xnyd);
	ynxd_bad = pg_mul_s32_overflow(y->numer, x->denom, &ynxd);
	numer_bad = pg_add_s32_overflow(xnyd, ynxd, &numer);
	denom_bad = pg_mul_s32_overflow(x->denom, y->denom, &denom);

	if (nxyd_bad || ynxd_bad || numer_bad || denom_bad)
	{
		/* overflow in intermediate value */
		if (!simplify(x) && !simplify(y))
		{
			/* neither fraction could reduce, cannot proceed */
			ereport(ERROR, (
							errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
							errmsg("intermediate value overflow in rational addition")
							));
		}
		/* the fraction(s) reduced, good for one more retry */
		goto retry_add;
	}
	result = palloc(sizeof(Rational));
	result->numer = numer;
	result->denom = denom;
	return result;
}

Rational *
mul(Rational * x, Rational * y)
{
	int32		numer,
				denom;
	bool		numer_bad,
				denom_bad;
	Rational   *result;

retry_mul:
	numer_bad = pg_mul_s32_overflow(x->numer, y->numer, &numer);
	denom_bad = pg_mul_s32_overflow(x->denom, y->denom, &denom);

	if (numer_bad || denom_bad)
	{
		/* overflow in intermediate value */
		if (!simplify(x) && !simplify(y))
		{
			/* neither fraction could reduce, cannot proceed */
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("intermediate value overflow in rational multiplication")));
		}
		/* the fraction(s) reduced, good for one more retry */
		goto retry_mul;
	}
	result = palloc(sizeof(Rational));
	result->numer = numer;
	result->denom = denom;

	return result;
}

void
mediant(Rational * x, Rational * y, Rational * m)
{
	/*
	 * Rational_intermediate sends fractions with small numers and denoms, and
	 * slowly builds up. The search will take forever before we ever get close
	 * to arithmetic overflow in this function, so I don't guard it here.
	 */
	m->numer = x->numer + y->numer;
	m->denom = x->denom + y->denom;
}
