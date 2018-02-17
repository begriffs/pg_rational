\echo Use "CREATE EXTENSION pg_rational" to load this file. \quit

CREATE FUNCTION rational_in(cstring)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION rational_out(rational)
RETURNS cstring
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION rational_recv(internal)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION rational_send(rational)
RETURNS bytea
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE rational (
  INPUT   = rational_in,
  OUTPUT  = rational_out,
  RECEIVE = rational_recv,
  SEND    = rational_send,
  INTERNALLENGTH = 16
);

CREATE FUNCTION rational_add(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR + (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_add,
  commutator = +
);

CREATE FUNCTION rational_sub(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_sub
);

CREATE FUNCTION rational_mul(rational, rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR * (
  leftarg = rational,
  rightarg = rational,
  procedure = rational_mul,
  commutator = *
);

CREATE FUNCTION rational_neg(rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - (
  rightarg = rational,
  procedure = rational_neg
);

CREATE FUNCTION rational_simplify(rational)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

------------- Comparison ------------- 

CREATE FUNCTION rational_eq(rational, rational)
RETURNS boolean
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
LANGUAGE C IMMUTABLE STRICT;

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
