CREATE FUNCTION rational_intermediate(rational, rational, integer)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE;
