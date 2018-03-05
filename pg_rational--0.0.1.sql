\echo Use "CREATE EXTENSION pg_rational" to load this file. \quit

CREATE FUNCTION rational_in(cstring)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_out(rational)
RETURNS cstring
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_recv(internal)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_send(rational)
RETURNS bytea
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE rational (
  INPUT   = rational_in,
  OUTPUT  = rational_out,
  RECEIVE = rational_recv,
  SEND    = rational_send,
  INTERNALLENGTH = 16
);

CREATE FUNCTION rational_create(bigint, bigint)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE ratt AS (n bigint, d bigint);
CREATE FUNCTION tuple_to_rational(ratt)
RETURNS rational AS $$
  SELECT rational_create($1.n,$1.d);
$$ LANGUAGE SQL;

CREATE CAST (ratt AS rational)
  WITH FUNCTION tuple_to_rational(ratt)
  AS IMPLICIT;

CREATE FUNCTION rational_embed32(integer)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_embed64(bigint)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE CAST (bigint AS rational)
  WITH FUNCTION rational_embed64(bigint)
  AS IMPLICIT;

CREATE CAST (integer AS rational)
  WITH FUNCTION rational_embed32(integer)
  AS IMPLICIT;

CREATE FUNCTION rational_add(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR + (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_add,
  commutator = +
);

CREATE FUNCTION rational_sub(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR - (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_sub
);

CREATE FUNCTION rational_mul(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR * (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_mul,
  commutator = *
);

CREATE FUNCTION rational_div(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR / (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_div
);

CREATE FUNCTION rational_neg(rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR - (
  rightarg = rational,
  procedure = rational_neg
);

CREATE FUNCTION rational_simplify(rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_intermediate(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE;

------------- Comparison ------------- 

CREATE FUNCTION rational_eq(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR = (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_eq,
  COMMUTATOR = '=',
  NEGATOR = '<>',
  RESTRICT = eqsel,
  JOIN = eqjoinsel,
  HASHES, MERGES
);

CREATE FUNCTION rational_ne(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR <> (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_ne,
  COMMUTATOR = '<>',
  NEGATOR = '=',
  RESTRICT = neqsel,
  JOIN = neqjoinsel
);

CREATE FUNCTION rational_lt(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR < (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_lt,
  COMMUTATOR = > ,
  NEGATOR = >= ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE FUNCTION rational_le(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR <= (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_le,
  COMMUTATOR = >= ,
  NEGATOR = > ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE FUNCTION rational_gt(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR > (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_gt,
  COMMUTATOR = < ,
  NEGATOR = <= ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE FUNCTION rational_ge(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR >= (
  LEFTARG = rational,
  RIGHTARG = rational,
  PROCEDURE = rational_ge,
  COMMUTATOR = <= ,
  NEGATOR = < ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE FUNCTION rational_cmp(rational, rational)
RETURNS integer
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR CLASS btree_rational_ops
DEFAULT FOR TYPE rational USING btree
AS
  OPERATOR 1 <  ,
  OPERATOR 2 <= ,
  OPERATOR 3 =  ,
  OPERATOR 4 >= ,
  OPERATOR 5 >  ,
  FUNCTION 1 rational_cmp(rational, rational);

CREATE FUNCTION rational_hash(rational)
RETURNS integer
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR CLASS hash_rational_ops
  DEFAULT FOR TYPE rational USING hash AS
    OPERATOR 1 = ,
    FUNCTION 1 rational_hash(rational);


------------- Aggregates ------------- 

CREATE FUNCTION rational_smaller(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION rational_larger(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE AGGREGATE min(rational)  (
    SFUNC = rational_smaller,
    STYPE = rational,
    SORTOP = <,
    COMBINEFUNC = rational_smaller,
	PARALLEL = SAFE
);

CREATE AGGREGATE max(rational)  (
    SFUNC = rational_larger,
    STYPE = rational,
    SORTOP = >,
    COMBINEFUNC = rational_larger,
	PARALLEL = SAFE
);

CREATE AGGREGATE sum (rational)
(
    SFUNC = rational_add,
    STYPE = rational,
    COMBINEFUNC = rational_add,
	PARALLEL = SAFE
);
