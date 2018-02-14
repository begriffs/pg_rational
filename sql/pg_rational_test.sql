create extension pg_rational;

-- input

-- Can parse a simple fraction
select '1/3'::rational;
-- Interprets negative numerator
select '-1/3'::rational;
-- Moves negative value from denom to numer
select '1/-3'::rational;
-- Double negative becomes positive
select '-1/-3'::rational;
-- no spaces
select '1 /3'::rational;
-- no single numbers
select '1'::rational;
-- no garbage
select ''::rational;
select 'sdfkjsdfj34984538'::rational;
