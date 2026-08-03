CREATE FUNCTION rational_intermediate(rational, rational, integer)
RETURNS rational
AS '$libdir/pg_rational'
LANGUAGE C IMMUTABLE;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_setting('server_version_num')::int >= 90600
	THEN
		EXECUTE $alter$
			ALTER FUNCTION rational_intermediate(rational, rational) PARALLEL SAFE;
			ALTER FUNCTION rational_intermediate(rational, rational, integer) PARALLEL SAFE;
		$alter$;
	END IF;
END
$$;
