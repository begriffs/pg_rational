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
